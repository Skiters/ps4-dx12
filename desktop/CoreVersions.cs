using System.Diagnostics;
using System.Security.Cryptography;
using System.Text.Json;

namespace PS4Desktop;

sealed class CoreVersion
{
    public string Id { get; set; } = "";
    public string Name { get; set; } = "";
    public DateTimeOffset BuildDate { get; set; }
    public string Branch { get; set; } = "";
    public string Commit { get; set; } = "";
    public bool Dirty { get; set; }
    public string Backend { get; set; } = "dx12";
    public string Description { get; set; } = "";
    public string Status { get; set; } = "Экспериментальное";
    public string Executable { get; set; } = "shadPS4-DX12-Core.exe";
    public string Sha256 { get; set; } = "";
    public Dictionary<string, string> Files { get; set; } = [];
    public string Results { get; set; } = "";
    [System.Text.Json.Serialization.JsonIgnore] public string Directory { get; set; } = "";
    [System.Text.Json.Serialization.JsonIgnore] public string? Error { get; set; }
    public override string ToString() => $"{Name} · {Status}" + (Error is null ? "" : " · повреждено");
}

static class CoreVersions
{
    public static string Root => Environment.GetEnvironmentVariable("PS4DESKTOP_CORE_ROOT") ?? Path.Combine(Settings.DataRoot, "cores");
    public static string Hash(string path)
    {
        using var stream = File.OpenRead(path);
        return Convert.ToHexStringLower(SHA256.HashData(stream));
    }
    static string Inside(CoreVersion version, string relative)
    {
        if (Path.IsPathRooted(relative)) throw new IOException("Абсолютный путь в пакете ядра");
        string path = Path.GetFullPath(Path.Combine(version.Directory, relative));
        if (!path.StartsWith(Path.GetFullPath(version.Directory) + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase))
            throw new IOException("Путь вне пакета ядра");
        return path;
    }
    public static List<CoreVersion> Discover()
    {
        if (!System.IO.Directory.Exists(Root)) return [];
        List<CoreVersion> result = [];
        foreach (string directory in System.IO.Directory.EnumerateDirectories(Root).Order())
        {
            string manifest = Path.Combine(directory, "core-version.json");
            if (!File.Exists(manifest)) continue;
            CoreVersion version;
            try
            {
                version = JsonSerializer.Deserialize<CoreVersion>(File.ReadAllText(manifest)) ?? throw new IOException("Пустой manifest");
                version.Directory = directory;
                if (version.Id != Path.GetFileName(directory) || string.IsNullOrWhiteSpace(version.Name))
                    throw new IOException("Некорректная идентичность версии");
            }
            catch (Exception ex)
            {
                version = new() { Id = Path.GetFileName(directory), Name = Path.GetFileName(directory), Directory = directory, Error = ex.Message };
            }
            result.Add(version);
        }
        return result;
    }
    public static string Validate(CoreVersion version)
    {
        if (version.Error is not null) throw new IOException(version.Error);
        if (version.Backend != "dx12") throw new IOException("Эта библиотека запускает только DX12-ядра.");
        string executable = Inside(version, version.Executable);
        if (!version.Files.TryGetValue(version.Executable, out var hash) || hash != version.Sha256 || hash.Length != 64)
            throw new IOException("В manifest отсутствует SHA256 исполняемого файла");
        foreach (var file in version.Files)
            if (!string.Equals(Hash(Inside(version, file.Key)), file.Value, StringComparison.OrdinalIgnoreCase))
                throw new IOException("Файл ядра повреждён: " + file.Key);
        return executable;
    }
    public static string Resolve(Settings settings)
    {
        if (string.IsNullOrEmpty(settings.SelectedCoreId)) return settings.NativeCore;
        var version = Discover().SingleOrDefault(v => v.Id == settings.SelectedCoreId);
        if (version is null) throw new IOException("Выбранная версия ядра отсутствует. Откройте «Версии ядра» и выберите доступную версию.");
        try { return Validate(version); }
        catch (Exception ex) { throw new IOException("Выбранная версия ядра недоступна. Откройте «Версии ядра» и выберите доступную версию. " + ex.Message, ex); }
    }
}

static class NativeGameLaunch
{
    public static ProcessStartInfo Create(Settings settings, string gameDirectory, string? captureRoot = null)
    {
        string executable = CoreVersions.Resolve(settings);
        string profile = NativeProfile.Ensure(settings);
        var start = new ProcessStartInfo(executable) { WorkingDirectory = profile, UseShellExecute = false };
        NativeLaunchPolicy.Apply(start);
        start.ArgumentList.Add("--game"); start.ArgumentList.Add(Path.Combine(gameDirectory, "eboot.bin"));
        if (captureRoot is not null)
        {
            string marker = Path.Combine(Path.GetFullPath(captureRoot), "capture.enabled");
            start.Environment["SHADPS4_NATIVE_DIAGNOSTICS"] = "1";
            start.Environment["SHADPS4_NATIVE_LOG_FILTER"] = "*:Warning Loader:Info";
            start.Environment["SHADPS4_FRAME_CAPTURE"] = marker;
            start.Environment["SHADPS4_NATIVE_CAPTURE_MARKER"] = marker;
            start.Environment["SHADPS4_DX12_MEMORY_PERF"] = "1";
            start.Environment["SHADPS4_DX12_BUFFER_BUDGET_MB"] = "2048";
            start.Environment["SHADPS4_DX12_PARTIAL_RENDER"] = "0";
            start.ArgumentList.Add("--fullscreen"); start.ArgumentList.Add("false");
            start.CreateNoWindow = true;
            start.RedirectStandardOutput = start.RedirectStandardError = true;
        }
        return start;
    }
    public static void Record(Settings settings, Process process, string destination)
    {
        System.IO.Directory.CreateDirectory(destination);
        File.WriteAllText(Path.Combine(destination, "launch-manifest.json"), JsonSerializer.Serialize(new {
            version = settings.SelectedCoreId, executable = process.StartInfo.FileName,
            sha256 = CoreVersions.Hash(process.StartInfo.FileName), pid = process.Id,
            creationTimeUtc = process.StartTime.ToUniversalTime(), profile = process.StartInfo.WorkingDirectory,
            arguments = process.StartInfo.ArgumentList.ToArray()
        }, new JsonSerializerOptions { WriteIndented = true }));
    }
}
