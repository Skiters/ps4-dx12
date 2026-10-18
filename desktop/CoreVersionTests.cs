using System.Text.Json;
using System.Text.Json.Nodes;
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
            CoreVersion Make(string id, string text, string backend)
            {
                string directory = Path.Combine(CoreVersions.Root, id); Directory.CreateDirectory(directory);
                string executable = Path.Combine(directory, "core.exe"); File.WriteAllText(executable, text);
                string hash = CoreVersions.Hash(executable);
                var version = new CoreVersion { Id = id, Name = id, Directory = directory, Executable = "core.exe", Sha256 = hash, Files = new() { ["core.exe"] = hash } };
                version.Backend = backend;
                version.ManifestVersion = 2;
                version.Commit = new string('a', 40);
                version.Source = new() { Repository = "https://github.com/shadps4-emu/shadPS4", Revision = version.Commit };
                File.WriteAllText(Path.Combine(directory, "compiler-profile.json"), "{\"configuration\":\"Release\"}");
                string profileHash = CoreVersions.Hash(Path.Combine(directory, "compiler-profile.json"));
                version.Files["compiler-profile.json"] = profileHash;
                File.WriteAllText(Path.Combine(directory, "native-contract.json"), "{}");
                version.Files["native-contract.json"] = CoreVersions.Hash(Path.Combine(directory, "native-contract.json"));
                version.Build = new() { Configuration = "Release", Architecture = "x64", Compiler = "clang-cl", CompilerVersion = "22.1.3",
                    CompilerProfileFile = "compiler-profile.json", CompilerProfileSha256 = profileHash, NativeCompilerContractFile = "native-contract.json" };
                File.WriteAllText(Path.Combine(directory, "core-version.json"), JsonSerializer.Serialize(version)); return version;
            }
            void Require(bool value) { if (!value) throw new IOException("Core version contract failed"); }
            var a = Make("baseline", "baseline", "vulkan"); var b = Make("final", "final", "dx12");
            Require(CoreVersions.Discover().Count == 2);
            // Persist and reload independently from the installed adjacent settings.
            var settings = new Settings { SelectedCoreId = b.Id };
            settings.Save(); settings = Settings.Load();
            Require(CoreVersions.Resolve(settings) == Path.Combine(b.Directory, b.Executable));
            settings.SelectedCoreId = a.Id;
            Require(CoreVersions.Resolve(settings) == Path.Combine(a.Directory, a.Executable));
            Require(CoreVersions.Backend(settings) == "vulkan");
            void Rejected(Action action) { bool rejected = false; try { action(); } catch (IOException) { rejected = true; } Require(rejected); }
            b.Backend = "unknown"; Rejected(() => CoreVersions.Validate(b)); b.Backend = "dx12";
            b.Source!.Revision = new string('b', 40); Rejected(() => CoreVersions.Validate(b)); b.Source.Revision = b.Commit;
            b.Build!.CompilerProfileSha256 = new string('0', 64); Rejected(() => CoreVersions.Validate(b)); b.Build.CompilerProfileSha256 = b.Files["compiler-profile.json"];
            b.Build.NativeCompilerContractFile = "missing.json"; Rejected(() => CoreVersions.Validate(b)); b.Build.NativeCompilerContractFile = "native-contract.json";
            string extra = Path.Combine(b.Directory, "extra.dll"); File.WriteAllText(extra, "untracked runtime");
            Rejected(() => CoreVersions.Validate(b)); File.Delete(extra);
            File.WriteAllText(Path.Combine(b.Directory, "native-contract.json"), "corrupt"); Rejected(() => CoreVersions.Validate(b));
            File.WriteAllText(Path.Combine(b.Directory, "native-contract.json"), "{}");
            b.ManifestVersion = 1; var sourceIdentity = b.Source; var buildIdentity = b.Build; b.Source = null; b.Build = null;
            Require(CoreVersions.Validate(b) == Path.Combine(b.Directory, b.Executable));
            b.ManifestVersion = 2; b.Source = sourceIdentity; b.Build = buildIdentity;
            ProfileContract(settings, a, b, Require, Rejected);
            ReferenceCaptureContract(a, Require, Rejected);
            settings.SelectedCoreId = a.Id;
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
    static void ReferenceCaptureContract(CoreVersion version, Action<bool> require, Action<Action> rejected)
    {
        string folder = Path.Combine(version.Directory, "reference-completion"); Directory.CreateDirectory(folder);
        string manifestPath = Path.Combine(folder, "completion-layer.json"), libraryPath = Path.Combine(folder, "guest-completion-layer.dll");
        File.WriteAllText(libraryPath, "capture observer fixture");
        File.WriteAllText(manifestPath, "{\"layer\":{\"name\":\"VK_LAYER_CODEX_guest_completion\",\"library_path\":\"guest-completion-layer.dll\"}}");
        File.WriteAllText(Path.Combine(version.Directory, "reference.map"), "map fixture");
        File.WriteAllText(Path.Combine(version.Directory, "reference.pdb"), "pdb fixture");
        var contract = new ReferenceCompletionContract { ReferenceRevision = version.Commit, ExecutableSha256 = version.Sha256,
            Symbol = "?Flip@VideoOutDriver@VideoOut@Libraries@@fixture", FlipRva = 4096, FlipSize = 64,
            FlipPresentReturnRva = 4128, RequestRegister = "rbx", RequestIndexOffset = 24,
            LayerName = "VK_LAYER_CODEX_guest_completion", LayerManifestFile = "reference-completion/completion-layer.json",
            LayerLibraryFile = "reference-completion/guest-completion-layer.dll", LayerManifestSha256 = CoreVersions.Hash(manifestPath),
            LayerLibrarySha256 = CoreVersions.Hash(libraryPath), MapFile = "reference.map", PdbFile = "reference.pdb",
            MapSha256 = CoreVersions.Hash(Path.Combine(version.Directory, "reference.map")), PdbSha256 = CoreVersions.Hash(Path.Combine(version.Directory, "reference.pdb")) };
        foreach (string file in new[] { contract.LayerManifestFile, contract.LayerLibraryFile, contract.MapFile, contract.PdbFile })
            version.Files[file] = CoreVersions.Hash(Path.Combine(version.Directory, file));
        version.Build!.ReferenceCompletionContractFile = "reference-completion-contract.json";
        void Save()
        {
            string path = Path.Combine(version.Directory, version.Build.ReferenceCompletionContractFile);
            File.WriteAllText(path, JsonSerializer.Serialize(contract)); version.Files[version.Build.ReferenceCompletionContractFile] = CoreVersions.Hash(path);
        }
        Save(); CoreVersions.Validate(version);
        var start = new System.Diagnostics.ProcessStartInfo(); CoreVersions.ReferenceCapture(version, start, Settings.DataRoot);
        require(start.Environment["VK_LAYER_PATH"] == folder && start.Environment["SHADPS4_REFERENCE_FLIP_RVA"] == "1000");
        contract.ExecutableSha256 = new string('0', 64); Save(); rejected(() => CoreVersions.ReferenceCapture(version, start, Settings.DataRoot));
        contract.ExecutableSha256 = version.Sha256;
        contract.FlipSize = 0; Save(); rejected(() => CoreVersions.ReferenceCapture(version, start, Settings.DataRoot)); contract.FlipSize = 64;
        contract.FlipPresentReturnRva = contract.FlipRva + contract.FlipSize; Save(); rejected(() => CoreVersions.ReferenceCapture(version, start, Settings.DataRoot)); contract.FlipPresentReturnRva = 4128;
        contract.RequestRegister = "rax"; Save(); rejected(() => CoreVersions.ReferenceCapture(version, start, Settings.DataRoot)); contract.RequestRegister = "rbx";
        contract.RequestIndexOffset = 16; Save(); rejected(() => CoreVersions.ReferenceCapture(version, start, Settings.DataRoot)); contract.RequestIndexOffset = 24; Save();
        File.WriteAllText(manifestPath, "{\"layer\":{\"name\":\"VK_LAYER_CODEX_guest_completion\",\"library_path\":\"../other.dll\"}}");
        version.Files[contract.LayerManifestFile] = contract.LayerManifestSha256 = CoreVersions.Hash(manifestPath); Save();
        rejected(() => CoreVersions.ReferenceCapture(version, start, Settings.DataRoot));
        File.WriteAllText(manifestPath, "{\"layer\":{\"name\":\"VK_LAYER_CODEX_guest_completion\",\"library_path\":\"guest-completion-layer.dll\"}}");
        version.Files[contract.LayerManifestFile] = contract.LayerManifestSha256 = CoreVersions.Hash(manifestPath); Save();
        File.WriteAllText(Path.Combine(version.Directory, "core-version.json"), JsonSerializer.Serialize(version));
    }
    static void ProfileContract(Settings settings, CoreVersion baseline, CoreVersion final, Action<bool> require, Action<Action> rejected)
    {
        settings.ProfileRoot = Path.Combine(Settings.DataRoot, "template");
        settings.NativeProfileRoot = Path.Combine(Settings.DataRoot, "dx12-profile");
        settings.VulkanProfileRoot = Path.Combine(Settings.DataRoot, "vulkan-profile");
        string user = Path.Combine(settings.ProfileRoot, "user"); Directory.CreateDirectory(user);
        var config = new JsonObject { ["General"] = new JsonObject { ["home_dir"] = Path.Combine(user, "home") },
            ["Vulkan"] = new JsonObject { ["pipeline_cache_enabled"] = true },
            ["GPU"] = new JsonObject { ["fsr_enabled"] = true, ["rcas_enabled"] = true, ["hdr_allowed"] = true } };
        string source = config.ToJsonString(); File.WriteAllText(Path.Combine(user, "config.json"), source);
        settings.SelectedCoreId = baseline.Id;
        var vulkan = NativeGameLaunch.Create(settings, Settings.DataRoot);
        settings.SelectedCoreId = final.Id;
        var dx12 = NativeGameLaunch.Create(settings, Settings.DataRoot);
        NativeGameLaunch.ValidateStart(vulkan); NativeGameLaunch.ValidateStart(dx12);
        var capture = NativeGameLaunch.Create(settings, Settings.DataRoot, Path.Combine(Settings.DataRoot, "capture"));
        NativeGameLaunch.ValidateStart(capture);
        require(capture.Environment["SHADPS4_NATIVE_DIAGNOSTICS"] == "0" && capture.Environment["SHADPS4_DX12_MEMORY_PERF"] == "0" &&
            capture.Environment["SHADPS4_DX12_INDEPENDENT_VBLANK"] == "0");
        dx12.ArgumentList.Add("unexpected"); rejected(() => NativeGameLaunch.ValidateStart(dx12)); dx12.ArgumentList.RemoveAt(dx12.ArgumentList.Count - 1);
        dx12.Environment["SHADPS4_DX12_BUFFER_BUDGET_MB"] = "48"; rejected(() => NativeGameLaunch.ValidateStart(dx12));
        dx12.Environment.Remove("SHADPS4_DX12_BUFFER_BUDGET_MB");
        NativeGameLaunch.ValidateStart(dx12);
        dx12.Environment["SHADPS4_DX12_BUFFER_BUDGET_MB"] = "48";
        dx12.Environment["SHADPS4_FRAME_CAPTURE"] = "stale-marker";
        NativeLaunchPolicy.Apply(dx12);
        require(!dx12.Environment.ContainsKey("SHADPS4_DX12_BUFFER_BUDGET_MB") && !dx12.Environment.ContainsKey("SHADPS4_FRAME_CAPTURE"));
        require(vulkan.WorkingDirectory == settings.VulkanProfileRoot && dx12.WorkingDirectory == settings.NativeProfileRoot);
        require(File.ReadAllText(Path.Combine(user, "config.json")) == source);
        foreach (string profile in new[] { vulkan.WorkingDirectory, dx12.WorkingDirectory })
        {
            var value = JsonNode.Parse(File.ReadAllText(Path.Combine(profile, "user", "config.json")))!;
            require(value["Vulkan"]!["pipeline_cache_enabled"]!.GetValue<bool>());
            foreach (string option in new[] { "fsr_enabled", "rcas_enabled", "hdr_allowed" }) require(value["GPU"]![option]!.GetValue<bool>());
            require(value["General"]!["home_dir"]!.GetValue<string>() == Path.Combine(profile, "user", "home"));
            require(NativeProfile.Ensure(settings, profile == dx12.WorkingDirectory ? "dx12" : "vulkan") == profile);
        }
        string sharedSave = Path.Combine(settings.NativeProfileRoot, "user", "home", "save.bin");
        Directory.CreateDirectory(Path.GetDirectoryName(sharedSave)!);
        File.WriteAllText(sharedSave, "original save");
        final.IsolatedProfile = true;
        string finalManifest = Path.Combine(final.Directory, "core-version.json");
        File.WriteAllText(finalManifest, JsonSerializer.Serialize(final));
        string sharedNativeProfile = settings.NativeProfileRoot;
        var isolated = NativeGameLaunch.Create(settings, Settings.DataRoot);
        string isolatedProfile = Path.Combine(Settings.DataRoot, "core-profiles", final.Id, "native-profile");
        require(isolated.WorkingDirectory == isolatedProfile && settings.NativeProfileRoot == sharedNativeProfile);
        string isolatedSave = Path.Combine(isolatedProfile, "user", "home", "save.bin");
        require(File.ReadAllText(isolatedSave) == "original save");
        File.WriteAllText(isolatedSave, "preview save");
        require(File.ReadAllText(sharedSave) == "original save");
        require(NativeGameLaunch.Create(settings, Settings.DataRoot).WorkingDirectory == isolatedProfile);
        require(File.ReadAllText(isolatedSave) == "preview save");
        NativeGameLaunch.ValidateStart(isolated);
        // Publication and launch use the same cross-process gate: a launcher
        // cannot validate old files and start while the package is being swapped.
        string packagePath = Path.TrimEndingDirectorySeparator(Path.GetFullPath(final.Directory));
        string gateKey = Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(
            System.Text.Encoding.UTF8.GetBytes(packagePath.ToUpperInvariant())));
        using var gateReady = new ManualResetEventSlim();
        using var releaseGate = new ManualResetEventSlim();
        var publisher = new Thread(() => {
            using var gate = new Mutex(false, @"Local\PS4Desktop-Core-" + gateKey);
            gate.WaitOne();
            try { gateReady.Set(); releaseGate.Wait(); }
            finally { gate.ReleaseMutex(); }
        });
        publisher.Start();
        try
        {
            require(gateReady.Wait(TimeSpan.FromSeconds(10)));
            using var blocked = new System.Diagnostics.Process { StartInfo = isolated };
            rejected(() => NativeGameLaunch.Start(blocked)); // Fake executable must never run.
        }
        finally { releaseGate.Set(); publisher.Join(); }
        // Replacing a stable current package must reuse the selected isolated
        // profile, including saves changed since its initial creation.
        final.ProfileId = final.Id;
        string stableProfileId = final.ProfileId;
        final.ProfileId = "../escape";
        rejected(() => CoreVersions.Validate(final));
        final.ProfileId = stableProfileId;
        File.WriteAllText(finalManifest, JsonSerializer.Serialize(final));
        require(NativeGameLaunch.Create(settings, Settings.DataRoot).WorkingDirectory == isolatedProfile);
        require(File.ReadAllText(isolatedSave) == "preview save");
        var migrated = JsonSerializer.Deserialize<CoreVersion>(JsonSerializer.Serialize(final))!;
        migrated.Id = "dx12-current";
        migrated.Directory = final.Directory;
        migrated.External = true;
        CoreVersions.Select(settings, migrated);
        require(NativeGameLaunch.Create(settings, Settings.DataRoot).WorkingDirectory == isolatedProfile);
        require(File.ReadAllText(isolatedSave) == "preview save");
        settings.ExternalCoreDirectory = "";
        settings.ExternalCore = null;
        settings.SelectedCoreId = final.Id;
        final.ProfileId = "";
        File.WriteAllText(finalManifest, JsonSerializer.Serialize(final));
        settings.SelectedCoreId = baseline.Id;
        require(NativeGameLaunch.Create(settings, Settings.DataRoot).WorkingDirectory == settings.VulkanProfileRoot);
        final.IsolatedProfile = false;
        File.WriteAllText(finalManifest, JsonSerializer.Serialize(final));
        settings.SelectedCoreId = final.Id;
        settings.VulkanProfileRoot = settings.NativeProfileRoot;
        rejected(() => NativeProfile.Ensure(settings));
    }
}
