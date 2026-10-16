using System.Text.Json;

namespace PS4Desktop;

// Filesystem/settings regression only. Never starts a core, device probe or UI.
static class ExternalCoreTests
{
    public static int Run()
    {
        string root = Path.Combine(Path.GetTempPath(), "ps4-external-core-" + Guid.NewGuid().ToString("N"));
        string? data = Environment.GetEnvironmentVariable("PS4DESKTOP_DATA_ROOT");
        string? catalog = Environment.GetEnvironmentVariable("PS4DESKTOP_CORE_ROOT");
        try
        {
            Environment.SetEnvironmentVariable("PS4DESKTOP_DATA_ROOT", Path.Combine(root, "settings"));
            Environment.SetEnvironmentVariable("PS4DESKTOP_CORE_ROOT", null);
            Directory.CreateDirectory(root);
            void Require(bool condition, string message) { if (!condition) throw new IOException(message); }
            void Reject(Action action)
            {
                bool rejected = false;
                try { action(); } catch (IOException) { rejected = true; }
                Require(rejected, "Invalid external core was accepted");
            }
            string folder = Path.Combine(root, "release core with spaces");
            string compiler = Path.Combine(folder, "native-compiler");
            Directory.CreateDirectory(compiler);
            string executable = Path.Combine(folder, "shadPS4-DX12-Core.exe");
            File.WriteAllText(executable, "test identity, not an executable");
            foreach (string name in new[] { "spirv_to_dxil.dll", "dxcompiler.dll", "dxil.dll" })
                File.WriteAllText(Path.Combine(compiler, name), name);
            string runtime = Path.Combine(folder, "SDL3.dll");
            File.WriteAllText(runtime, "runtime identity");
            string[] before = Directory.GetFiles(folder, "*", SearchOption.AllDirectories).Order().ToArray();
            var core = CoreVersions.FromDirectory(folder);
            var settings = new Settings();
            CoreVersions.Select(settings, core); settings.Save(); settings = Settings.Load();
            Require(CoreVersions.Resolve(settings) == executable && CoreVersions.Backend(settings) == "dx12", "Folder selection did not survive reload");
            Require(settings.SelectedCoreId == "" && settings.ExternalCoreDirectory == folder &&
                CoreVersions.Selected(settings)!.IsolatedProfile, "External selection lost identity/profile isolation");
            Require(!Directory.Exists(CoreVersions.Root) && before.SequenceEqual(
                Directory.GetFiles(folder, "*", SearchOption.AllDirectories).Order()), "Folder selection copied or wrote a core package");
            settings.ProfileRoot = Path.Combine(root, "profile-template");
            settings.NativeProfileRoot = Path.Combine(root, "native-profile");
            Directory.CreateDirectory(Path.Combine(settings.ProfileRoot, "user"));
            File.WriteAllText(Path.Combine(settings.ProfileRoot, "user", "config.json"), "{\"General\":{}}");
            var launch = NativeGameLaunch.Create(settings, root); // Build parameters only; never start the fake executable.
            NativeGameLaunch.ValidateStart(launch);
            File.WriteAllText(runtime, "changed runtime"); Reject(() => CoreVersions.Resolve(settings));
            File.Delete(runtime); Reject(() => CoreVersions.Resolve(settings)); File.WriteAllText(runtime, "runtime identity");
            string added = Path.Combine(folder, "extra.dll"); File.WriteAllText(added, "unselected runtime");
            Reject(() => CoreVersions.Resolve(settings)); Reject(() => NativeGameLaunch.ValidateStart(launch)); File.Delete(added);
            File.WriteAllText(executable, "changed executable"); Reject(() => CoreVersions.Resolve(settings));
            File.WriteAllText(executable, "test identity, not an executable");
            string moved = Path.Combine(root, "renamed release"); Directory.Move(folder, moved);
            Reject(() => CoreVersions.Resolve(settings));
            CoreVersions.Select(settings, CoreVersions.FromDirectory(moved));
            Require(CoreVersions.Resolve(settings) == Path.Combine(moved, core.Executable), "Reselecting a moved folder failed");
            string dependency = Path.Combine(moved, "native-compiler", "dxil.dll");
            File.Delete(dependency); Reject(() => CoreVersions.FromDirectory(moved)); File.WriteAllText(dependency, "dxil.dll");
            string manifestFolder = Path.Combine(root, "arbitrary package folder name"); Directory.CreateDirectory(manifestFolder);
            string vulkan = Path.Combine(manifestFolder, "reference.exe"); File.WriteAllText(vulkan, "reference identity");
            string hash = CoreVersions.Hash(vulkan);
            var package = new CoreVersion { Id = "reference-id", Name = "Reference", Backend = "vulkan", Executable = "reference.exe",
                Sha256 = hash, Files = new() { ["reference.exe"] = hash } };
            string manifest = Path.Combine(manifestFolder, "core-version.json");
            File.WriteAllText(manifest, JsonSerializer.Serialize(package));
            CoreVersions.Select(settings, CoreVersions.FromDirectory(manifestFolder)); settings.Save(); settings = Settings.Load();
            Require(CoreVersions.Backend(settings) == "vulkan" && CoreVersions.Resolve(settings) == vulkan, "Manifest backend/path was lost");
            package.Executable = "../escape.exe"; File.WriteAllText(manifest, JsonSerializer.Serialize(package));
            Reject(() => CoreVersions.FromDirectory(manifestFolder));
            package.Executable = "reference.exe"; package.Id = "../escape";
            File.WriteAllText(manifest, JsonSerializer.Serialize(package)); Reject(() => CoreVersions.FromDirectory(manifestFolder));
            settings.ExternalCore = null; Reject(() => CoreVersions.Resolve(settings));
            package.Id = "installed-reference"; package.Directory = Path.Combine(CoreVersions.Root, package.Id);
            Directory.CreateDirectory(package.Directory); File.Copy(vulkan, Path.Combine(package.Directory, package.Executable));
            File.WriteAllText(Path.Combine(package.Directory, "core-version.json"), JsonSerializer.Serialize(package));
            CoreVersions.Select(settings, package);
            Require(settings.ExternalCoreDirectory == "" && settings.ExternalCore is null &&
                CoreVersions.Resolve(settings) == Path.Combine(package.Directory, package.Executable), "Catalogue selection did not clear the external selection");
            Console.WriteLine("PASS: external folder selection, persistence, immutable hashes, dependencies, relocation, manifest backend, containment and no core copying");
            return 0;
        }
        catch (Exception error) { Console.Error.WriteLine(error); return 1; }
        finally
        {
            Environment.SetEnvironmentVariable("PS4DESKTOP_DATA_ROOT", data);
            Environment.SetEnvironmentVariable("PS4DESKTOP_CORE_ROOT", catalog);
            if (Directory.Exists(root)) Directory.Delete(root, true);
        }
    }
}
