using System.Text.Json;
namespace PS4Desktop;
static class CoreVersionTests
{
    public static int Run()
    {
        string previous = Settings.DataRoot;
        string? previousEnvironment = Environment.GetEnvironmentVariable("PS4DESKTOP_DATA_ROOT");
        string? previousCoreRoot = Environment.GetEnvironmentVariable("PS4DESKTOP_CORE_ROOT");
        string root = Path.Combine(Path.GetTempPath(), "ps4-core-test-" + Guid.NewGuid().ToString("N"));
        Environment.SetEnvironmentVariable("PS4DESKTOP_DATA_ROOT", root);
        Environment.SetEnvironmentVariable("PS4DESKTOP_CORE_ROOT", null);
        try
        {
            Directory.CreateDirectory(root);
            CoreVersion Make(string id, string text)
            {
                string directory = Path.Combine(CoreVersions.Root, id); Directory.CreateDirectory(directory);
                string executable = Path.Combine(directory, "core.exe"); File.WriteAllText(executable, text);
                string hash = CoreVersions.Hash(executable);
                var version = new CoreVersion { Id = id, Name = id, Directory = directory, Executable = "core.exe", Sha256 = hash, Files = new() { ["core.exe"] = hash } };
                File.WriteAllText(Path.Combine(directory, "core-version.json"), JsonSerializer.Serialize(version)); return version;
            }
            void Require(bool value) { if (!value) throw new IOException("Core version contract failed"); }
            var a = Make("baseline", "baseline"); var b = Make("final", "final");
            Require(CoreVersions.Discover().Count == 2);
            // Persist and reload independently from the installed adjacent settings.
            var settings = new Settings { SelectedCoreId = b.Id };
            settings.Save(); settings = Settings.Load();
            Require(CoreVersions.Resolve(settings) == Path.Combine(b.Directory, b.Executable));
            settings.SelectedCoreId = a.Id;
            Require(CoreVersions.Resolve(settings) == Path.Combine(a.Directory, a.Executable));
            void Rejected(Action action) { bool rejected = false; try { action(); } catch (IOException) { rejected = true; } Require(rejected); }
            File.WriteAllText(Path.Combine(a.Directory, a.Executable), "corrupt");
            Rejected(() => CoreVersions.Resolve(settings));
            settings.SelectedCoreId = "missing"; Rejected(() => CoreVersions.Resolve(settings));
            b.Executable = "../escape.exe"; Rejected(() => CoreVersions.Validate(b));
            File.WriteAllText(Path.Combine(CoreVersions.Root, "final", "core-version.json"), "invalid json");
            Require(CoreVersions.Discover().Single(v => v.Id == "final").Error is not null);
            File.WriteAllText(Path.Combine(previous, "core-versions-test.json"), "{\"passed\":true}");
            return 0;
        }
        finally
        {
            Environment.SetEnvironmentVariable("PS4DESKTOP_DATA_ROOT", previousEnvironment);
            Environment.SetEnvironmentVariable("PS4DESKTOP_CORE_ROOT", previousCoreRoot);
            Directory.Delete(root, true);
        }
    }
}
