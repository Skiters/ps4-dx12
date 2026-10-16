using System.Diagnostics;
using System.Security.Cryptography;
using System.Text.Json;
using System.Runtime.CompilerServices;

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
    public bool IsolatedProfile { get; set; }
    public int ManifestVersion { get; set; } = 1;
    public CoreSourceIdentity? Source { get; set; }
    public CoreBuildIdentity? Build { get; set; }
    [System.Text.Json.Serialization.JsonIgnore] public string Directory { get; set; } = "";
    [System.Text.Json.Serialization.JsonIgnore] public string? Error { get; set; }
    public override string ToString() => $"{Name} · {Status}" + (Error is null ? "" : " · повреждено");
}

sealed class CoreSourceIdentity
{
    public string Repository { get; set; } = "";
    public string Revision { get; set; } = "";
    public bool Dirty { get; set; }
    public string PatchSha256 { get; set; } = "";
    public string PatchFile { get; set; } = "";
    public Dictionary<string, string> Submodules { get; set; } = [];
    public string ReferenceRepository { get; set; } = "";
    public string ReferenceRevision { get; set; } = "";
}

sealed class CoreBuildIdentity
{
    public string Configuration { get; set; } = "";
    public string Architecture { get; set; } = "";
    public string Compiler { get; set; } = "";
    public string CompilerVersion { get; set; } = "";
    public string CompilerProfileSha256 { get; set; } = "";
    public string CompilerProfileFile { get; set; } = "";
    public string NativeCompilerContractFile { get; set; } = "";
    public string ReferenceCompletionContractFile { get; set; } = "";
}

sealed class ReferenceCompletionContract
{
    public string ReferenceRevision { get; set; } = "";
    public string Symbol { get; set; } = "";
    public uint FlipRva { get; set; }
    public uint FlipSize { get; set; }
    public uint FlipPresentReturnRva { get; set; }
    public string RequestRegister { get; set; } = "";
    public uint RequestIndexOffset { get; set; }
    public string ExecutableSha256 { get; set; } = "";
    public string MapFile { get; set; } = "";
    public string MapSha256 { get; set; } = "";
    public string PdbFile { get; set; } = "";
    public string PdbSha256 { get; set; } = "";
    public string LayerName { get; set; } = "";
    public string LayerManifestFile { get; set; } = "";
    public string LayerManifestSha256 { get; set; } = "";
    public string LayerLibraryFile { get; set; } = "";
    public string LayerLibrarySha256 { get; set; } = "";
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
        if (string.IsNullOrWhiteSpace(relative) || Path.IsPathRooted(relative)) throw new IOException("Некорректный путь в пакете ядра");
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
        if (version.Backend is not ("dx12" or "vulkan")) throw new IOException("Неизвестный backend ядра.");
        if (version.ManifestVersion is < 1 or > 2) throw new IOException("Неизвестная версия manifest ядра.");
        if (version.ManifestVersion == 2) ValidateIdentity(version);
        string executable = Inside(version, version.Executable);
        if (!version.Files.TryGetValue(version.Executable, out var hash) || hash != version.Sha256 || hash.Length != 64)
            throw new IOException("В manifest отсутствует SHA256 исполняемого файла");
        foreach (var file in version.Files)
            if (!Hex(file.Value, 64) || !string.Equals(Hash(Inside(version, file.Key)), file.Value, StringComparison.OrdinalIgnoreCase))
                throw new IOException("Файл ядра повреждён: " + file.Key);
        if (version.ManifestVersion == 2)
            foreach (string runtime in System.IO.Directory.EnumerateFiles(version.Directory, "*", SearchOption.AllDirectories)
                .Where(path => Path.GetExtension(path).Equals(".dll", StringComparison.OrdinalIgnoreCase) || Path.GetExtension(path).Equals(".exe", StringComparison.OrdinalIgnoreCase)))
                if (!version.Files.Keys.Any(path => Inside(version, path).Equals(runtime, StringComparison.OrdinalIgnoreCase)))
                    throw new IOException("Файл runtime отсутствует в manifest: " + Path.GetRelativePath(version.Directory, runtime));
        return executable;
    }
    static bool Hex(string value, int size) => value.Length == size && value.All(Uri.IsHexDigit);
    static void ValidateIdentity(CoreVersion version)
    {
        var source = version.Source ?? throw new IOException("В manifest отсутствует идентичность исходников.");
        var build = version.Build ?? throw new IOException("В manifest отсутствует идентичность сборки.");
        if (string.IsNullOrWhiteSpace(source.Repository) || !Hex(source.Revision, 40) ||
            source.Revision != version.Commit || source.Dirty != version.Dirty ||
            (source.Dirty && !Hex(source.PatchSha256, 64)) ||
            source.Submodules is null || source.Submodules.Any(entry => string.IsNullOrWhiteSpace(entry.Key) || !Hex(entry.Value, 40)) ||
            (!string.IsNullOrEmpty(source.ReferenceRevision) && !Hex(source.ReferenceRevision, 40)))
            throw new IOException("Некорректная идентичность исходников.");
        if (string.IsNullOrWhiteSpace(build.Configuration) || build.Architecture != "x64" ||
            string.IsNullOrWhiteSpace(build.Compiler) || string.IsNullOrWhiteSpace(build.CompilerVersion) ||
            !Hex(build.CompilerProfileSha256, 64) ||
            !version.Files.TryGetValue(build.CompilerProfileFile, out var profileHash) ||
            profileHash != build.CompilerProfileSha256)
            throw new IOException("Некорректная идентичность компилятора.");
        if (source.Dirty && (!version.Files.TryGetValue(source.PatchFile, out var patchHash) || patchHash != source.PatchSha256))
            throw new IOException("В пакете отсутствует точный patch исходников.");
        if (version.Backend == "dx12" &&
            (string.IsNullOrEmpty(build.NativeCompilerContractFile) ||
             !version.Files.ContainsKey(build.NativeCompilerContractFile)))
            throw new IOException("В пакете отсутствует контракт native shader compiler.");
        if (!string.IsNullOrEmpty(build.ReferenceCompletionContractFile) &&
            (version.Backend != "vulkan" || !version.Files.ContainsKey(build.ReferenceCompletionContractFile)))
            throw new IOException("Некорректный контракт измерения reference.");
    }
    public static void ReferenceCapture(CoreVersion version, ProcessStartInfo start, string destination)
    {
        string contractFile = version.Build?.ReferenceCompletionContractFile ?? "";
        if (version.Backend != "vulkan" || string.IsNullOrEmpty(contractFile) || !version.Files.ContainsKey(contractFile))
            throw new IOException("У версии ядра отсутствует проверяемый контракт GPU completion reference.");
        var contract = JsonSerializer.Deserialize<ReferenceCompletionContract>(File.ReadAllText(Inside(version, contractFile)),
            new JsonSerializerOptions { PropertyNameCaseInsensitive = true }) ?? throw new IOException("Пустой контракт reference.");
        if (contract.ReferenceRevision != version.Source?.Revision || contract.ExecutableSha256 != version.Sha256 ||
            !contract.Symbol.StartsWith("?Flip@VideoOutDriver@VideoOut@Libraries@@", StringComparison.Ordinal) ||
            contract.FlipRva == 0 || contract.FlipSize == 0 || contract.FlipSize > uint.MaxValue - contract.FlipRva ||
            contract.FlipPresentReturnRva <= contract.FlipRva || contract.FlipPresentReturnRva >= contract.FlipRva + contract.FlipSize ||
            contract.RequestRegister != "rbx" || contract.RequestIndexOffset != 24 ||
            contract.LayerName != "VK_LAYER_CODEX_guest_completion" ||
            !version.Files.TryGetValue(contract.MapFile, out var mapHash) || mapHash != contract.MapSha256 ||
            !version.Files.TryGetValue(contract.PdbFile, out var pdbHash) || pdbHash != contract.PdbSha256 ||
            !version.Files.TryGetValue(contract.LayerManifestFile, out var manifestHash) || manifestHash != contract.LayerManifestSha256 ||
            !version.Files.TryGetValue(contract.LayerLibraryFile, out var libraryHash) || libraryHash != contract.LayerLibrarySha256)
            throw new IOException("Контракт измерения не соответствует выбранному reference binary.");
        string manifest = Inside(version, contract.LayerManifestFile), library = Inside(version, contract.LayerLibraryFile);
        using var document = JsonDocument.Parse(File.ReadAllText(manifest));
        var layer = document.RootElement.GetProperty("layer");
        string relative = layer.GetProperty("library_path").GetString() ?? "";
        if (layer.GetProperty("name").GetString() != contract.LayerName || Path.IsPathRooted(relative) ||
            Path.GetFullPath(Path.Combine(Path.GetDirectoryName(manifest)!, relative)) != library)
            throw new IOException("Manifest измерения указывает другую библиотеку.");
        start.Environment["VK_LAYER_PATH"] = Path.GetDirectoryName(manifest)!;
        start.Environment["VK_INSTANCE_LAYERS"] = contract.LayerName;
        start.Environment["SHADPS4_REFERENCE_COMPLETION_LOG"] = Path.Combine(destination, "reference-completed-frames.jsonl");
        start.Environment["SHADPS4_REFERENCE_FLIP_RVA"] = contract.FlipRva.ToString("x");
        start.Environment["SHADPS4_REFERENCE_FLIP_SIZE"] = contract.FlipSize.ToString("x");
        start.Environment["SHADPS4_REFERENCE_FLIP_RETURN_RVA"] = contract.FlipPresentReturnRva.ToString("x");
        start.Environment["SHADPS4_REFERENCE_REQUEST_REGISTER"] = contract.RequestRegister;
        start.Environment["SHADPS4_REFERENCE_REQUEST_INDEX_OFFSET"] = contract.RequestIndexOffset.ToString("x");
    }
    public static CoreVersion? Selected(Settings settings)
    {
        if (string.IsNullOrEmpty(settings.SelectedCoreId)) return null;
        return Discover().SingleOrDefault(v => v.Id == settings.SelectedCoreId)
            ?? throw new IOException("Выбранная версия ядра отсутствует. Откройте «Версии ядра» и выберите доступную версию.");
    }
    public static string Backend(Settings settings) => Selected(settings)?.Backend ?? "dx12";
    public static string Resolve(Settings settings)
    {
        if (string.IsNullOrEmpty(settings.SelectedCoreId)) return settings.NativeCore;
        var version = Selected(settings)!;
        try { return Validate(version); }
        catch (Exception ex) { throw new IOException("Выбранная версия ядра недоступна. Откройте «Версии ядра» и выберите доступную версию. " + ex.Message, ex); }
    }
}

static class NativeGameLaunch
{
    sealed record Identity(string Backend, string Executable, string Sha256, string Profile, string[] Arguments,
        Dictionary<string, string?> CaptureEnvironment, CoreVersion? Core);
    static readonly ConditionalWeakTable<ProcessStartInfo, Identity> identities = new();
    public static ProcessStartInfo Create(Settings settings, string gameDirectory, string? captureRoot = null)
    {
        var selected = CoreVersions.Selected(settings);
        string executable = selected is null ? CoreVersions.Resolve(settings) : CoreVersions.Validate(selected);
        string backend = selected?.Backend ?? "dx12";
        if (selected?.IsolatedProfile == true)
        {
            string sourceProfile = backend == "dx12" ? settings.NativeProfileRoot : settings.VulkanProfileRoot;
            var isolated = JsonSerializer.Deserialize<Settings>(JsonSerializer.Serialize(settings))
                ?? throw new IOException("Не удалось подготовить настройки отдельного профиля ядра.");
            string root = Path.GetFullPath(Path.Combine(Settings.DataRoot, "core-profiles"));
            string versionRoot = Path.GetFullPath(Path.Combine(root, selected.Id));
            if (!versionRoot.StartsWith(root + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase))
                throw new IOException("Профиль версии находится вне каталога данных.");
            isolated.ProfileRoot = Directory.Exists(sourceProfile) ? sourceProfile : settings.ProfileRoot;
            isolated.NativeProfileRoot = Path.Combine(versionRoot, "native-profile");
            isolated.VulkanProfileRoot = Path.Combine(versionRoot, "vulkan-profile");
            settings = isolated;
        }
        string profile = NativeProfile.Ensure(settings, backend);
        var start = new ProcessStartInfo(executable) { WorkingDirectory = profile, UseShellExecute = false };
        NativeLaunchPolicy.Apply(start);
        start.ArgumentList.Add("--game"); start.ArgumentList.Add(Path.Combine(gameDirectory, "eboot.bin"));
        if (captureRoot is not null)
        {
            captureRoot = Path.GetFullPath(captureRoot);
            System.IO.Directory.CreateDirectory(captureRoot);
            if (System.IO.Directory.EnumerateFileSystemEntries(captureRoot).Any())
                throw new IOException("Каталог измерения должен быть пустым: " + captureRoot);
            string marker = Path.Combine(Path.GetFullPath(captureRoot), "capture.enabled");
            start.Environment["SHADPS4_NATIVE_DIAGNOSTICS"] = "0";
            start.Environment["SHADPS4_NATIVE_LOG_FILTER"] = "*:Warning Loader:Info";
            start.Environment["SHADPS4_FRAME_CAPTURE"] = marker;
            start.Environment["SHADPS4_GPU_IDENTITY_LOG"] = Path.Combine(captureRoot, "gpu-identity.jsonl");
            if (backend == "dx12") start.Environment["SHADPS4_RUNTIME_MODULE_LOG"] = Path.Combine(captureRoot, "compiler-modules.jsonl");
            if (backend == "dx12") start.Environment["SHADPS4_FRAME_TRACE_LOG"] = Path.Combine(captureRoot, "guest-frames.csv");
            start.Environment["SHADPS4_NATIVE_CAPTURE_MARKER"] = marker;
            start.Environment["SHADPS4_DX12_MEMORY_PERF"] = "0";
            start.Environment["SHADPS4_DX12_INDEPENDENT_VBLANK"] = "0";
            start.Environment["SHADPS4_DX12_PARTIAL_RENDER"] = "0";
            start.ArgumentList.Add("--fullscreen"); start.ArgumentList.Add("false");
            start.CreateNoWindow = true;
            start.RedirectStandardOutput = start.RedirectStandardError = true;
            if (backend == "vulkan") CoreVersions.ReferenceCapture(selected ?? throw new IOException("Измерение reference требует пакет ядра."), start, captureRoot);
        }
        var snapshot = selected is null ? null : JsonSerializer.Deserialize<CoreVersion>(JsonSerializer.Serialize(selected));
        if (snapshot is not null) snapshot.Directory = selected!.Directory;
        identities.Add(start, new(backend, Path.GetFullPath(executable), CoreVersions.Hash(executable), profile,
            start.ArgumentList.ToArray(), start.Environment.Where(entry => entry.Key.StartsWith("SHADPS4_", StringComparison.OrdinalIgnoreCase) ||
                entry.Key.StartsWith("VK_", StringComparison.OrdinalIgnoreCase)).ToDictionary(entry => entry.Key, entry => entry.Value), snapshot));
        return start;
    }
    public static void ValidateStart(ProcessStartInfo start)
    {
        if (!identities.TryGetValue(start, out var identity) || start.UseShellExecute ||
            Path.GetFullPath(start.FileName) != identity.Executable || start.WorkingDirectory != identity.Profile ||
            !start.ArgumentList.SequenceEqual(identity.Arguments) || CoreVersions.Hash(identity.Executable) != identity.Sha256)
            throw new IOException("Параметры запуска изменились после проверки пакета ядра.");
        foreach (var entry in identity.CaptureEnvironment)
            if (!start.Environment.TryGetValue(entry.Key, out var value) || value != entry.Value)
                throw new IOException("Параметры измерения изменились после проверки ядра.");
        if (start.Environment.Count(entry => entry.Key.StartsWith("SHADPS4_", StringComparison.OrdinalIgnoreCase) ||
            entry.Key.StartsWith("VK_", StringComparison.OrdinalIgnoreCase)) != identity.CaptureEnvironment.Count)
            throw new IOException("В запуск добавлены непроверенные параметры измерения.");
        if (identity.Core is not null) CoreVersions.Validate(identity.Core);
    }
    public static bool Start(Process process)
    {
        ValidateStart(process.StartInfo);
        return process.Start();
    }
    public static Task<bool> ObserveRuntime(Process process, string destination)
    {
        if (!identities.TryGetValue(process.StartInfo, out var identity))
            throw new IOException("Процесс запущен без проверенного пакета ядра.");
        return Task.Run(async () =>
        {
            var expected = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            if (identity.Core is not null)
                foreach (var entry in identity.Core.Files.Where(entry => entry.Key.EndsWith(".dll", StringComparison.OrdinalIgnoreCase) ||
                    entry.Key.EndsWith(".exe", StringComparison.OrdinalIgnoreCase)))
                    expected[Path.GetFullPath(Path.Combine(identity.Core.Directory, entry.Key))] = entry.Value;
            string[] required = identity.Backend == "dx12" ? ["d3d12.dll", "dxgi.dll"] :
                ["vulkan-1.dll", "guest-completion-layer.dll"];
            string[] pinned = ["spirv_to_dxil.dll", "dxcompiler.dll", "dxil.dll", "amd_ags_x64.dll", "guest-completion-layer.dll"];
            var known = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            var previous = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            bool valid = true, everReady = false;
            long nativeOffset = 0;
            await using var stream = new FileStream(Path.Combine(destination, "loaded-runtime.jsonl"), FileMode.CreateNew, FileAccess.Write, FileShare.Read);
            await using var writer = new StreamWriter(stream);
            async Task Write(object value) { await writer.WriteLineAsync(JsonSerializer.Serialize(value)); await writer.FlushAsync(); }
            async Task Observe(string path, string sha256, string source, long? baseAddress = null, int? imageBytes = null)
            {
                path = Path.GetFullPath(path); string name = Path.GetFileName(path);
                if (sha256.Length != 64 || !sha256.All(Uri.IsHexDigit)) throw new IOException("Loaded module evidence has an invalid SHA256");
                if (known.Count >= 4096 && !known.ContainsKey(path)) throw new IOException("Loaded module evidence exceeded its bound");
                string? error = null;
                if (expected.TryGetValue(path, out var expectedHash) && sha256 != expectedHash)
                    error = "Loaded package module differs from its validated hash";
                else if (pinned.Contains(name, StringComparer.OrdinalIgnoreCase) && !expected.ContainsKey(path))
                    error = "Pinned compiler/observer module was loaded outside the validated package";
                else if (identity.Core is not null && path.StartsWith(identity.Core.Directory.TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar,
                    StringComparison.OrdinalIgnoreCase) && !expected.ContainsKey(path))
                    error = "Untracked module was loaded from the selected package";
                else if (identity.Backend == "dx12" && name.Equals("vulkan-1.dll", StringComparison.OrdinalIgnoreCase))
                    error = "Native DX12 process loaded Vulkan";
                if (error is not null) valid = false;
                if (known.TryGetValue(path, out var oldHash) && oldHash == sha256 && error is null) return;
                known[path] = sha256;
                await Write(new { type = "module-loaded", source, utc = DateTimeOffset.UtcNow, pid = process.Id,
                    path, sha256, baseAddress, imageBytes, valid, error });
            }
            while (!process.HasExited)
            {
                try
                {
                    process.Refresh();
                    var current = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                    foreach (ProcessModule module in process.Modules)
                    {
                        string path = Path.GetFullPath(module.FileName);
                        current.Add(path);
                        if (previous.Contains(path)) continue;
                        await Observe(path, CoreVersions.Hash(path), "process-modules", module.BaseAddress.ToInt64(), module.ModuleMemorySize);
                    }
                    foreach (string path in previous.Except(current, StringComparer.OrdinalIgnoreCase))
                        await Write(new { type = "module-unloaded", utc = DateTimeOffset.UtcNow, pid = process.Id, path, valid });
                    if (identity.Backend == "dx12")
                    {
                        string nativeLog = Path.Combine(destination, "compiler-modules.jsonl");
                        if (File.Exists(nativeLog))
                        {
                            await using var nativeFile = new FileStream(nativeLog, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
                            if (nativeFile.Length < nativeOffset || nativeFile.Length > 64 * 1024 * 1024)
                                throw new IOException("Native module evidence was truncated or exceeded its bound");
                            nativeFile.Position = nativeOffset;
                            using var nativeReader = new StreamReader(nativeFile, System.Text.Encoding.UTF8, detectEncodingFromByteOrderMarks: false, leaveOpen: true);
                            string content = await nativeReader.ReadToEndAsync(); int end = content.LastIndexOf('\n');
                            if (end >= 0)
                            {
                                foreach (string line in content[..end].Split('\n', StringSplitOptions.RemoveEmptyEntries))
                                {
                                    using var entry = JsonDocument.Parse(line);
                                    var item = entry.RootElement;
                                    if (item.GetProperty("pid").GetInt32() != process.Id || item.GetProperty("source").GetString() != "native-load")
                                        throw new IOException("Native module evidence belongs to another process or source");
                                    await Observe(item.GetProperty("path").GetString()!, item.GetProperty("sha256").GetString()!, "native-load",
                                        unchecked((long)item.GetProperty("baseAddress").GetUInt64()));
                                }
                                nativeOffset += System.Text.Encoding.UTF8.GetByteCount(content[..(end + 1)]);
                            }
                        }
                    }
                    bool ready = required.All(name => known.Keys.Any(path => Path.GetFileName(path).Equals(name, StringComparison.OrdinalIgnoreCase)));
                    if (ready && !everReady)
                    {
                        everReady = true;
                        await Write(new { type = "runtime-loaded", utc = DateTimeOffset.UtcNow, pid = process.Id, backend = identity.Backend, valid });
                    }
                    previous = current;
                }
                catch (Exception error) when (error is System.ComponentModel.Win32Exception or InvalidOperationException)
                {
                    if (process.HasExited) break;
                    valid = false;
                    await Write(new { type = "module-enumeration-error", utc = DateTimeOffset.UtcNow, pid = process.Id, valid, error = error.Message });
                }
                catch (Exception error) when (error is IOException or UnauthorizedAccessException or JsonException or KeyNotFoundException or FormatException)
                {
                    valid = false;
                    await Write(new { type = "module-identity-error", utc = DateTimeOffset.UtcNow, pid = process.Id, valid, error = error.Message });
                }
                await Task.Delay(500);
            }
            valid &= everReady;
            await Write(new { type = "runtime-stop", utc = DateTimeOffset.UtcNow, pid = process.Id, valid, everReady, observedModules = known });
            return valid;
        });
    }
    public static void Record(Settings settings, Process process, string destination)
    {
        if (!identities.TryGetValue(process.StartInfo, out var identity))
            throw new IOException("Запуск не прошёл проверку каталога ядер.");
        System.IO.Directory.CreateDirectory(destination);
        string? actualExecutable = null, actualSha256 = null, identityError = null;
        DateTime? creationTimeUtc = null;
        try
        {
            creationTimeUtc = process.StartTime.ToUniversalTime();
            actualExecutable = process.MainModule?.FileName;
            if (actualExecutable is null || Path.GetFullPath(actualExecutable) != identity.Executable)
                throw new IOException("Дочерний процесс загрузил другой исполняемый файл.");
            actualSha256 = CoreVersions.Hash(actualExecutable);
            if (actualSha256 != identity.Sha256) throw new IOException("Файл запущенного ядра изменился.");
        }
        catch (Exception ex) { identityError = ex.Message; }
        File.WriteAllText(Path.Combine(destination, "launch-manifest.json"), JsonSerializer.Serialize(new {
            version = identity.Core?.Id ?? "", backend = identity.Backend, executable = identity.Executable,
            sha256 = identity.Sha256, core = identity.Core, pid = process.Id,
            creationTimeUtc, profile = identity.Profile,
            arguments = identity.Arguments, captureEnvironment = identity.CaptureEnvironment,
            actualExecutable, actualSha256, processImageVerified = identityError is null, identityError
        }, new JsonSerializerOptions { WriteIndented = true }));
        if (identityError is not null) throw new IOException("Не удалось подтвердить образ дочернего процесса: " + identityError);
    }
}
