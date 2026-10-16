using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace PS4Desktop;

sealed class Settings
{
    public static string DataRoot => Path.GetFullPath(Environment.GetEnvironmentVariable("PS4DESKTOP_DATA_ROOT")
        ?? Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "PS4Desktop"));
    public string Backend { get; set; } = "dx12";
    public string SelectedCoreId { get; set; } = "";
    public string ExternalCoreDirectory { get; set; } = "";
    public CoreVersion? ExternalCore { get; set; }
    public string NativeCore { get; set; } = Path.Combine(AppContext.BaseDirectory, "native", "shadPS4-DX12.exe");
    public string ProfileRoot { get; set; } = Path.Combine(AppContext.BaseDirectory, "profile-template");
    public string NativeProfileRoot { get; set; } = Path.Combine(DataRoot, "native-profile");
    public string VulkanProfileRoot { get; set; } = Path.Combine(DataRoot, "vulkan-profile");
    public string InstallRoot { get; set; } = Path.Combine(DataRoot, "Games");
    public List<string> LibraryRoots { get; set; } = [Path.Combine(DataRoot, "Games")];
    public string Python { get; set; } = "python";
    public static string FilePath => Environment.GetEnvironmentVariable("PS4DESKTOP_DATA_ROOT") is null && File.Exists(Path.Combine(AppContext.BaseDirectory, "settings.json"))
        ? Path.Combine(AppContext.BaseDirectory, "settings.json") : Path.Combine(DataRoot, "settings.json");
    public static Settings Load() => File.Exists(FilePath)
        ? JsonSerializer.Deserialize<Settings>(File.ReadAllText(FilePath)) ?? new() : new();
    public void Save()
    {
        Directory.CreateDirectory(Path.GetDirectoryName(FilePath)!);
        var temp = FilePath + ".tmp";
        File.WriteAllText(temp, JsonSerializer.Serialize(this, new JsonSerializerOptions { WriteIndented = true }));
        File.Move(temp, FilePath, true);
    }
}

static class NativeProfile
{
    public static string Ensure(Settings settings, string backend = "dx12")
    {
        if (backend is not ("dx12" or "vulkan")) throw new IOException("Неизвестный backend профиля.");
        string target = Path.GetFullPath(backend == "dx12" ? settings.NativeProfileRoot : settings.VulkanProfileRoot);
        string other = Path.GetFullPath(backend == "dx12" ? settings.VulkanProfileRoot : settings.NativeProfileRoot);
        string source = Path.GetFullPath(settings.ProfileRoot);
        string sourcePrefix=source.TrimEnd(Path.DirectorySeparatorChar)+Path.DirectorySeparatorChar;
        string targetPrefix=target.TrimEnd(Path.DirectorySeparatorChar)+Path.DirectorySeparatorChar;
        string otherPrefix=other.TrimEnd(Path.DirectorySeparatorChar)+Path.DirectorySeparatorChar;
        if (sourcePrefix.StartsWith(targetPrefix,StringComparison.OrdinalIgnoreCase)||targetPrefix.StartsWith(sourcePrefix,StringComparison.OrdinalIgnoreCase))
            throw new IOException("Ядру нужен отдельный профиль, чтобы сохранить исходные настройки и сохранения.");
        if (otherPrefix.StartsWith(targetPrefix,StringComparison.OrdinalIgnoreCase)||targetPrefix.StartsWith(otherPrefix,StringComparison.OrdinalIgnoreCase))
            throw new IOException("Профили DX12 и Vulkan должны использовать разные каталоги.");
        if (Directory.Exists(target))
        {
            string configPath = Path.Combine(target, "user", "config.json");
            if (!File.Exists(configPath))
                throw new IOException("В профиле ядра отсутствует user/config.json: " + target);
            string expectedHome = Path.GetFullPath(Path.Combine(target, "user", "home"));
            var existing = JsonNode.Parse(File.ReadAllText(configPath));
            string? configuredHome = existing?["General"]?["home_dir"]?.GetValue<string>();
            if (string.IsNullOrWhiteSpace(configuredHome) ||
                !string.Equals(Path.GetFullPath(configuredHome, target), expectedHome, StringComparison.OrdinalIgnoreCase))
                throw new IOException("Профиль ядра должен использовать отдельную папку сохранений: " + expectedHome);
            return target;
        }
        string staging = target + ".init-" + Guid.NewGuid().ToString("N");
        Directory.CreateDirectory(Path.GetDirectoryName(target)!);
        try
        {
            string user = Path.Combine(staging, "user");
            Directory.CreateDirectory(user);
            foreach (string entry in new[] { "config.json", "keys.json", "users.json", "home", "addcont", "custom_configs", "input_config", "patches", "sys_modules", "custom_modules", "fonts", "licenses", "trophy", "custom_trophy" })
                Copy(Path.Combine(source, "user", entry), Path.Combine(user, entry));
            string config = Path.Combine(user, "config.json");
            var document = JsonNode.Parse(File.ReadAllText(config)) ?? throw new IOException("Пустой профиль shadPS4");
            var general = document["General"] as JsonObject ?? throw new IOException("В профиле отсутствует General");
            general["home_dir"] = Path.Combine(target, "user", "home");
            general["addon_install_dir"] = Path.Combine(target, "user", "addcont");
            general["sys_modules_dir"] = Path.Combine(target, "user", "sys_modules");
            general["font_dir"] = Path.Combine(target, "user", "fonts");
            general["install_dirs"] = JsonSerializer.SerializeToNode(new[] { new { path = settings.InstallRoot, enabled = true } });
            File.WriteAllText(config, document.ToJsonString(new JsonSerializerOptions { WriteIndented = true }));
            string custom = Path.Combine(user, "custom_configs");
            if (Directory.Exists(custom)) foreach (string path in Directory.EnumerateFiles(custom, "*.json"))
            {
                var value = JsonNode.Parse(File.ReadAllText(path));
                if (value?["General"] is not JsonObject overrides) continue;
                if (overrides.ContainsKey("home_dir")) overrides["home_dir"] = Path.Combine(target, "user", "home");
                if (overrides.ContainsKey("addon_install_dir")) overrides["addon_install_dir"] = Path.Combine(target, "user", "addcont");
                File.WriteAllText(path, value.ToJsonString(new JsonSerializerOptions { WriteIndented = true }));
            }
            Directory.Move(staging, target);
            return target;
        }
        finally { if (Directory.Exists(staging)) Directory.Delete(staging, true); }
    }
    static void Copy(string source, string target)
    {
        if (!File.Exists(source) && !Directory.Exists(source)) return;
        if ((File.GetAttributes(source) & FileAttributes.ReparsePoint) != 0)
            throw new IOException("Ссылка в исходном профиле требует отдельной настройки: " + source);
        if (File.Exists(source)) { File.Copy(source, target); return; }
        Directory.CreateDirectory(target);
        foreach (string child in Directory.EnumerateFileSystemEntries(source)) Copy(child, Path.Combine(target, Path.GetFileName(child)));
    }
}

record Game(string Title, string Id, string Version, string Directory);

static class NativeLaunchPolicy
{
    // Library launches never inherit flags from an earlier diagnostic session.
    // Explicit diagnostic tools may still opt in when starting the core directly.
    public static void Apply(ProcessStartInfo start)
    {
        foreach (string name in start.Environment.Keys.Where(name => name.StartsWith("SHADPS4_", StringComparison.OrdinalIgnoreCase)).ToArray())
            start.Environment.Remove(name);
    }
}

record NativeStatus(string Adapter, bool GameLaunchReady);
static class NativeConnection
{
    public static async Task<NativeStatus> Probe(Settings settings)
    {
        if (CoreVersions.Backend(settings) != "dx12") throw new IOException("Проверка DX12-устройства недоступна для выбранного Vulkan-ядра.");
        // Saved full cores implement the game CLI, while --probe belongs to the
        // bundled connection host. Validate the selected package first, then
        // probe the device through that host without starting another game.
        _ = CoreVersions.Resolve(settings);
        string core = settings.NativeCore;
        if (!File.Exists(core)) throw new FileNotFoundException("Нативное DX12-ядро не найдено", core);
        var start = new ProcessStartInfo(core) { UseShellExecute = false, CreateNoWindow = true,
            RedirectStandardOutput = true, RedirectStandardError = true, StandardOutputEncoding = Encoding.UTF8 };
        NativeLaunchPolicy.Apply(start);
        start.ArgumentList.Add("--probe");
        using var process = Process.Start(start) ?? throw new IOException("Не удалось запустить DX12-ядро");
        using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(15));
        try
        {
            var output = process.StandardOutput.ReadToEndAsync(timeout.Token);
            var errors = process.StandardError.ReadToEndAsync(timeout.Token);
            await process.WaitForExitAsync(timeout.Token);
            using var document = JsonDocument.Parse(await output);
            string stderr = await errors;
            var value = document.RootElement;
            if (process.ExitCode != 0) throw new IOException(value.TryGetProperty("error", out var error) ? error.GetString() : stderr);
            if (value.GetProperty("protocol").GetInt32() != 1 || value.GetProperty("backend").GetString() != "dx12" ||
                !value.GetProperty("initialized").GetBoolean() || !value.GetProperty("presentationVerified").GetBoolean() ||
                value.GetProperty("vulkanLoaded").GetBoolean()) throw new IOException("Нативное ядро вернуло несовместимый результат проверки");
            return new(value.GetProperty("adapter").GetString() ?? "DX12", value.GetProperty("gameLaunchReady").GetBoolean());
        }
        catch
        {
            if (!process.HasExited) process.Kill(entireProcessTree: true);
            throw;
        }
    }
}

static class Library
{
    public static Dictionary<string, string> ReadSfo(string path)
    {
        var bytes = File.ReadAllBytes(path);
        using var stream = new MemoryStream(bytes);
        using var reader = new BinaryReader(stream);
        if (bytes.Length < 20 || reader.ReadUInt32() != 0x46535000) throw new InvalidDataException("Некорректный SFO");
        reader.ReadUInt32();
        uint keys = reader.ReadUInt32(), data = reader.ReadUInt32(), count = reader.ReadUInt32();
        if (count > 4096 || 20UL + 16UL * count > (ulong)bytes.Length) throw new InvalidDataException("Неполный SFO");
        var result = new Dictionary<string, string>();
        for (int i = 0; i < count; ++i)
        {
            stream.Position = 20 + 16 * i;
            ushort offset = reader.ReadUInt16(), format = reader.ReadUInt16();
            uint length = reader.ReadUInt32(); reader.ReadUInt32(); uint value = reader.ReadUInt32();
            long k = (long)keys + offset, v = (long)data + value;
            if (k >= bytes.Length || v > bytes.Length || length > bytes.Length - v) throw new InvalidDataException("Повреждённый SFO");
            int end = Array.IndexOf(bytes, (byte)0, (int)k);
            if (end < 0) throw new InvalidDataException("Повреждённый ключ SFO");
            if (format == 0x0204)
                result[Encoding.UTF8.GetString(bytes, (int)k, end - (int)k)] =
                    Encoding.UTF8.GetString(bytes, (int)v, (int)length).TrimEnd('\0');
        }
        return result;
    }

    public static List<Game> Scan(IEnumerable<string> roots)
    {
        var games = new List<Game>();
        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var root in roots.Distinct(StringComparer.OrdinalIgnoreCase))
        {
            if (!System.IO.Directory.Exists(root)) continue;
            // At most two levels: no recursive scan of game assets or shader caches.
            IEnumerable<string> candidates = [root];
            try { candidates = candidates.Concat(System.IO.Directory.GetDirectories(root)).ToArray(); }
            catch (UnauthorizedAccessException) { continue; }
            foreach (var folder in candidates)
            {
                var full = Path.GetFullPath(folder);
                if (!seen.Add(full) || !File.Exists(Path.Combine(full, "eboot.bin")) ||
                    full.EndsWith("-UPDATE", StringComparison.OrdinalIgnoreCase) || full.EndsWith("-patch", StringComparison.OrdinalIgnoreCase)) continue;
                try
                {
                    var sfo = ReadSfo(Path.Combine(full, "sce_sys", "param.sfo"));
                    // A merged base+patch installation can retain CATEGORY=gp.
                    // Standalone patch folders were already excluded by suffix.
                    if (sfo.GetValueOrDefault("CATEGORY", "gd") == "ac") continue;
                    games.Add(new(sfo.GetValueOrDefault("TITLE", Path.GetFileName(full)),
                        sfo.GetValueOrDefault("TITLE_ID", ""), sfo.GetValueOrDefault("APP_VER", ""), full));
                }
                catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or ArgumentException) { }
            }
        }
        return games.OrderBy(g => g.Title).ToList();
    }
}

sealed class MainWindow : Form
{
    Settings settings;
    readonly ListView library = new() { Dock = DockStyle.Fill, View = View.LargeIcon, MultiSelect = false, HideSelection = false, BackColor = Color.FromArgb(25, 29, 38), ForeColor = Color.White };
    readonly ImageList covers = new() { ImageSize = new(176, 100), ColorDepth = ColorDepth.Depth32Bit };
    readonly Label status = new() { Dock = DockStyle.Bottom, Height = 36, TextAlign = ContentAlignment.MiddleLeft };
    readonly Label backend = new() { Dock = DockStyle.Top, Height = 44, TextAlign = ContentAlignment.MiddleLeft, ForeColor = Color.LightSteelBlue };
    readonly ProgressBar progress = new() { Dock = DockStyle.Bottom, Height = 8 };
    readonly FlowLayoutPanel toolbar = new() { Dock = DockStyle.Top, AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink, Padding = new(8) };
    readonly TextBox search = new() { Dock = DockStyle.Top, PlaceholderText = "Поиск игры" };
    readonly System.Windows.Forms.Timer pad = new() { Interval = 65 };
    readonly Button playButton;
    List<Game> games = [];
    Process? worker, gameProcess;
    string? cancelPath;
    bool busy, scanning, launching;
    bool nativeProbing, nativeReady;
    ushort previousButtons;
    internal bool ExperimentalLaunchEnabled => playButton.Enabled;

    public MainWindow(Settings value)
    {
        settings = value;
        Text = "PS4 DX12 — Experimental · библиотека игр";
        BackColor = Color.FromArgb(18, 21, 28); ForeColor = Color.White;
        Font = new("Segoe UI", 12); MinimumSize = new(1000, 650); Size = new(1280, 800);
        StartPosition = FormStartPosition.CenterScreen;
        library.LargeImageList = covers;
        Button AddButton(string text, Action click)
        {
            var b = new Button { Text = text, AutoSize = true, Height = 42, FlatStyle = FlatStyle.Flat, BackColor = Color.FromArgb(40, 49, 65) };
            b.Click += (_, _) => click(); toolbar.Controls.Add(b); return b;
        }
        playButton = AddButton("▶ Играть · A", Launch);
        AddButton("Установить PKG · X", async () => await Install());
        AddButton("Добавить папку", AddFolder);
        AddButton("Обновить · Y", async () => await RefreshLibrary());
        AddButton("Настройки", Configure);
        AddButton("Версии ядра", SelectCore);
        AddButton("Проверить ядро", async () => await ProbeNative());
        AddButton("Отмена", Cancel);
        search.TextChanged += (_, _) => RenderLibrary();
        library.ItemActivate += (_, _) => Launch();
        Controls.Add(library); Controls.Add(progress); Controls.Add(status); Controls.Add(search); Controls.Add(backend); Controls.Add(toolbar);
        UpdateBackend();
        Shown += async (_, _) => { await RefreshLibrary(); await ProbeNative(); library.Focus(); };
        FormClosing += (_, e) =>
        {
            if (busy) { e.Cancel = true; Cancel(); status.Text = "Отмена установки… Дождитесь завершения, затем закройте окно."; }
        };
        pad.Tick += (_, _) => PollPad(); pad.Start();
        FormClosed += (_, _) => { pad.Dispose(); covers.Dispose(); gameProcess?.Dispose(); };
    }

    void UpdateBackend()
    {
        try
        {
            string selectedBackend = CoreVersions.Backend(settings);
            string selected = CoreVersions.Selected(settings)?.ToString() ?? "Ранее настроенное ядро";
            backend.Text = "  " + selected + " · " + (nativeReady ? "Подключение проверено." : "Экспериментальный запуск.");
            playButton.Text = "▶ Запустить " + (selectedBackend == "dx12" ? "DX12" : "Vulkan") + " · A";
            playButton.Enabled = true;
        }
        catch (IOException ex) { backend.Text = "  " + ex.Message; playButton.Enabled = false; }
    }
    async Task ProbeNative()
    {
        if (nativeProbing) return;
        nativeProbing = true; nativeReady = false; UpdateBackend(); status.Text = "Проверка выбранного ядра…";
        try
        {
            if (CoreVersions.Backend(settings) == "vulkan")
            {
                _ = CoreVersions.Resolve(settings);
                UpdateBackend(); status.Text = "Файлы Vulkan-ядра проверены. Устройство проверяется при запуске игры.";
                return;
            }
            var result = await NativeConnection.Probe(settings);
            nativeReady = result.GameLaunchReady;
            UpdateBackend();
            backend.Text += " · " + result.Adapter;
            status.Text = "DX12-устройство и вывод кадра проверены. Vulkan не загружен.";
        }
        catch (Exception ex) { status.Text = "Ошибка подключения ядра: " + ex.Message; }
        finally { nativeProbing = false; }
    }
    void Error(Exception ex) => MessageBox.Show(this, ex.Message, "PS4", MessageBoxButtons.OK, MessageBoxIcon.Error);
    async Task RefreshLibrary()
    {
        if (scanning) return;
        scanning = true; status.Text = "Чтение библиотеки…";
        try
        {
            var roots = settings.LibraryRoots.Append(settings.InstallRoot).ToArray();
            games = await Task.Run(() => Library.Scan(roots));
            RenderLibrary(); status.Text = $"Игр: {games.Count} · A — запуск · X — PKG · Y — обновить · LB/RB — меню";
        }
        catch (Exception ex) { Error(ex); }
        finally { scanning = false; }
    }

    void RenderLibrary()
    {
        string? selected = library.SelectedItems.Count > 0 ? (library.SelectedItems[0].Tag as Game)?.Directory : null;
        library.BeginUpdate(); library.Items.Clear();
        // Retain decoded thumbnails between search edits. Load each cover once.
        foreach (var game in games.Where(g => (g.Title + " " + g.Id).Contains(search.Text, StringComparison.OrdinalIgnoreCase)))
        {
            if (!covers.Images.ContainsKey(game.Directory))
            {
                using var thumbnail = new Bitmap(176, 100);
                using var graphics = Graphics.FromImage(thumbnail);
                graphics.Clear(Color.FromArgb(42, 53, 75));
                var icon = Path.Combine(game.Directory, "sce_sys", "icon0.png");
                try
                {
                    if (File.Exists(icon)) { using var image = Image.FromFile(icon); graphics.DrawImage(image, new Rectangle(0, 0, 176, 100)); }
                }
                catch (Exception ex) when (ex is IOException or ArgumentException or OutOfMemoryException) { }
                covers.Images.Add(game.Directory, thumbnail);
            }
            var item = new ListViewItem(game.Title + "\n" + game.Id + " · " + game.Version) { Tag = game, ImageKey = game.Directory, ToolTipText = game.Directory };
            library.Items.Add(item);
            if (game.Directory == selected) item.Selected = true;
        }
        if (library.Items.Count > 0 && library.SelectedItems.Count == 0) library.Items[0].Selected = true;
        library.EndUpdate();
    }

    void AddFolder()
    {
        using var dialog = new FolderBrowserDialog { Description = "Папка с играми или отдельная игра с eboot.bin" };
        if (dialog.ShowDialog(this) != DialogResult.OK) return;
        settings.LibraryRoots.Add(dialog.SelectedPath); settings.LibraryRoots = settings.LibraryRoots.Distinct(StringComparer.OrdinalIgnoreCase).ToList();
        settings.Save(); _ = RefreshLibrary();
    }

    void SelectCore()
    {
        if (busy || launching || gameProcess is { HasExited: false }) return;
        using var dialog = new Form { Text = "Версии ядра", Size = new(1050, 650), StartPosition = FormStartPosition.CenterParent, Font = Font };
        var versions = new ListBox { Dock = DockStyle.Top, Height = 200, HorizontalScrollbar = true };
        var details = new TextBox { Dock = DockStyle.Fill, Multiline = true, ReadOnly = true, ScrollBars = ScrollBars.Vertical };
        var select = new Button { Text = "Выбрать ядро", Dock = DockStyle.Bottom, Height = 45, Enabled = false };
        var browse = new Button { Text = "Выбрать папку с ядром…", Dock = DockStyle.Bottom, Height = 45 };
        var legacy = new Button { Text = "Вернуться к ранее настроенному ядру", Dock = DockStyle.Bottom, Height = 45 };
        foreach (var version in CoreVersions.Discover()) versions.Items.Add(version);
        if (!string.IsNullOrEmpty(settings.ExternalCoreDirectory))
            try { versions.Items.Add(CoreVersions.Selected(settings)!); } catch (IOException) { }
        browse.Click += (_, _) =>
        {
            using var folder = new FolderBrowserDialog { Description = "Папка с исполняемым файлом ядра и его DLL", ShowNewFolderButton = false,
                SelectedPath = settings.ExternalCoreDirectory };
            if (folder.ShowDialog(dialog) != DialogResult.OK) return;
            try
            {
                var version = CoreVersions.FromDirectory(folder.SelectedPath);
                for (int i = versions.Items.Count - 1; i >= 0; --i)
                    if (versions.Items[i] is CoreVersion existing && existing.External &&
                        existing.Directory.Equals(version.Directory, StringComparison.OrdinalIgnoreCase)) versions.Items.RemoveAt(i);
                versions.Items.Add(version); versions.SelectedItem = version;
            }
            catch (Exception ex) { MessageBox.Show(dialog, ex.Message, "Не удалось выбрать ядро"); }
        };
        versions.SelectedIndexChanged += (_, _) =>
        {
            if (versions.SelectedItem is not CoreVersion version) return;
            string validation;
            try { validation = CoreVersions.Validate(version); select.Enabled = true; }
            catch (Exception ex) { validation = ex.Message; select.Enabled = false; }
            details.Text = $"{version.Name}\r\nПапка: {version.Directory}\r\nСтатус: {version.Status}\r\nСохранения: {(version.IsolatedProfile ? "отдельная копия для этой версии" : "общий профиль выбранного backend")}\r\nДата сборки: {version.BuildDate:O}\r\nBackend: {version.Backend}\r\nВетка: {version.Branch}\r\nCommit: {version.Commit}\r\nИзменения поверх commit: {version.Dirty}\r\nSHA256: {version.Sha256}\r\nИсточник: {version.Source?.Repository}\r\nЭталон: {version.Source?.ReferenceRevision}\r\nКомпилятор: {version.Build?.Compiler} {version.Build?.CompilerVersion}\r\n{validation}\r\n\r\n{version.Description}\r\n\r\n{version.Results}";
        };
        select.Click += (_, _) =>
        {
            try
            {
                var version = (CoreVersion)versions.SelectedItem!;
                CoreVersions.Select(settings, version); nativeReady = false; settings.Save();
                dialog.DialogResult = DialogResult.OK;
            }
            catch (Exception ex) { MessageBox.Show(dialog, ex.Message); }
        };
        legacy.Click += (_, _) => { settings.SelectedCoreId = ""; settings.ExternalCoreDirectory = ""; settings.ExternalCore = null;
            settings.Backend = "dx12"; nativeReady = false; settings.Save(); dialog.DialogResult = DialogResult.OK; };
        dialog.Controls.Add(details); dialog.Controls.Add(versions); dialog.Controls.Add(select); dialog.Controls.Add(browse); dialog.Controls.Add(legacy);
        for (int i = 0; i < versions.Items.Count; ++i)
            if (((CoreVersion)versions.Items[i]).External ? !string.IsNullOrEmpty(settings.ExternalCoreDirectory) :
                string.IsNullOrEmpty(settings.ExternalCoreDirectory) && ((CoreVersion)versions.Items[i]).Id == settings.SelectedCoreId) versions.SelectedIndex = i;
        if (dialog.ShowDialog(this) == DialogResult.OK) { UpdateBackend(); _ = ProbeNative(); }
    }

    void Configure()
    {
        if (busy) return;
        using var form = new Form { Text = "Настройки библиотеки", Size = new(900, 600), MinimumSize = new(800, 550), AutoScroll = true, StartPosition = FormStartPosition.CenterParent, Font = Font, BackColor = BackColor, ForeColor = ForeColor };
        var panel = new TableLayoutPanel { Dock = DockStyle.Top, ColumnCount = 2, Padding = new(16), AutoSize = true };
        panel.ColumnStyles.Add(new(SizeType.Absolute, 215)); panel.ColumnStyles.Add(new(SizeType.Percent, 100));
        TextBox Field(string caption, string value)
        {
            var input = new TextBox { Text = value, Dock = DockStyle.Fill };
            panel.Controls.Add(new Label { Text = caption, AutoSize = true }); panel.Controls.Add(input); return input;
        }
        var nativeCore = Field("Нативное DX12-ядро", settings.NativeCore);
        panel.Controls.Add(new Label { Text = "Режим запуска", AutoSize = true });
        var mode = new ComboBox { Dock = DockStyle.Fill, DropDownStyle = ComboBoxStyle.DropDownList };
        mode.Items.Add("Нативный DirectX 12");
        mode.SelectedIndex = 0;
        panel.Controls.Add(mode);
        var profile = Field("Рабочий каталог / user", settings.ProfileRoot);
        var nativeProfile = Field("Отдельный профиль DX12", settings.NativeProfileRoot);
        var vulkanProfile = Field("Отдельный профиль Vulkan", settings.VulkanProfileRoot);
        var install = Field("Каталог установки PKG", settings.InstallRoot);
        bool bundled = File.Exists(Path.Combine(AppContext.BaseDirectory, "installer", "PS4PackageInstaller.exe"));
        var runtime = Field("Установщик PKG", bundled ? "Встроенный" : settings.Python);
        runtime.ReadOnly = bundled;
        var note = new Label { Text = "Нативный DX12: экспериментальный запуск доступен. Рендерер ещё в разработке; возможны ошибки и завершение игры.", AutoSize = true, MaximumSize = new(520, 80) };
        panel.Controls.Add(new Label()); panel.Controls.Add(note);
        var save = new Button { Text = "Сохранить", AutoSize = true }; panel.Controls.Add(new Label()); panel.Controls.Add(save);
        save.Click += (_, _) =>
        {
            try
            {
                if ((!bundled && !File.Exists(runtime.Text)) || !Directory.Exists(profile.Text)) throw new IOException("Проверьте пути к исходному профилю и установщику.");
                Directory.CreateDirectory(install.Text);
                if (!File.Exists(nativeCore.Text)) throw new IOException("Укажите существующее нативное DX12-ядро.");
                settings.ProfileRoot = Path.GetFullPath(profile.Text);
                settings.NativeProfileRoot = Path.GetFullPath(nativeProfile.Text);
                settings.VulkanProfileRoot = Path.GetFullPath(vulkanProfile.Text);
                if (!string.Equals(settings.NativeCore, Path.GetFullPath(nativeCore.Text), StringComparison.OrdinalIgnoreCase)) settings.SelectedCoreId = "";
                settings.NativeCore = Path.GetFullPath(nativeCore.Text); settings.Backend = CoreVersions.Backend(settings);
                settings.InstallRoot = Path.GetFullPath(install.Text); if (!bundled) settings.Python = Path.GetFullPath(runtime.Text);
                settings.Save(); form.DialogResult = DialogResult.OK;
            }
            catch (Exception ex) { MessageBox.Show(form, ex.Message); }
        };
        form.Controls.Add(panel); if (form.ShowDialog(this) == DialogResult.OK) { UpdateBackend(); _ = ProbeNative(); _ = RefreshLibrary(); }
    }

    async void Launch()
    {
        if (busy || launching || library.SelectedItems.Count == 0) return;
        if (gameProcess is { HasExited: false }) { status.Text = "Игра уже запущена."; return; }
        try
        {
            launching = true;
            var game = (Game)library.SelectedItems[0].Tag!;
            status.Text = "Загрузка игры… Подготовка отдельного профиля " + CoreVersions.Backend(settings);
            var start = await Task.Run(() => NativeGameLaunch.Create(settings, game.Directory));
            gameProcess?.Dispose(); gameProcess = new Process { StartInfo = start, EnableRaisingEvents = true };
            var launched = gameProcess;
            gameProcess.Exited += (_, _) =>
            {
                int exitCode = launched.ExitCode;
                if (!IsDisposed && IsHandleCreated) BeginInvoke(() => { status.Text = exitCode == 0 ? "Игра завершена. Библиотека готова." : $"Ядро завершилось с ошибкой {exitCode}."; Activate(); });
            };
            if (!NativeGameLaunch.Start(gameProcess)) throw new IOException("Не удалось запустить ядро");
            NativeGameLaunch.Record(settings, gameProcess, Settings.DataRoot);
            status.Text = "Запущено: " + game.Title;
        }
        catch (Exception ex) { Error(ex); }
        finally { launching = false; }
    }

    async Task<JsonElement> RunWorker(string mode, string package)
    {
        if (mode == "install") await Task.Run(() => NativeProfile.Ensure(settings));
        string bundled = Path.Combine(AppContext.BaseDirectory, "installer", "PS4PackageInstaller.exe");
        var start = new ProcessStartInfo(File.Exists(bundled) ? bundled : settings.Python) { UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true, RedirectStandardError = true, StandardOutputEncoding = Encoding.UTF8, StandardErrorEncoding = Encoding.UTF8 };
        start.Environment["PYTHONUTF8"] = "1";
        if (!File.Exists(bundled)) { start.ArgumentList.Add("-u"); start.ArgumentList.Add(Path.Combine(AppContext.BaseDirectory, "installer", "pkg_worker.py")); }
        foreach (var arg in new[] { mode, package,
                     "--root", settings.InstallRoot, "--roots", JsonSerializer.Serialize(settings.LibraryRoots),
                     "--addon", Path.Combine(settings.NativeProfileRoot, "user", "addcont"), "--cancel", cancelPath! }) start.ArgumentList.Add(arg);
        using var process = Process.Start(start) ?? throw new IOException("Не удалось запустить установщик");
        worker = process;
        var errors = process.StandardError.ReadToEndAsync();
        JsonElement last = default;
        while (await process.StandardOutput.ReadLineAsync() is { } line)
        {
            using var json = JsonDocument.Parse(line); last = json.RootElement.Clone();
            if (last.GetProperty("type").GetString() == "progress")
            {
                long total = last.GetProperty("total").GetInt64(), done = last.GetProperty("done").GetInt64();
                progress.Style = ProgressBarStyle.Continuous;
                progress.Value = total == 0 ? 0 : (int)Math.Clamp(done * 100.0 / total, 0, 100);
                status.Text = $"Установка: {progress.Value}% · {last.GetProperty("file").GetString()}";
            }
        }
        await process.WaitForExitAsync(); worker = null;
        string stderr = await errors;
        if (process.ExitCode != 0) throw new IOException(last.ValueKind == JsonValueKind.Object && last.TryGetProperty("message", out var message) ? message.GetString() : stderr);
        return last;
    }

    async Task Install()
    {
        if (busy) return;
        if (gameProcess is { HasExited: false }) { status.Text = "Устанавливайте пакеты после завершения игры."; return; }
        using var dialog = new OpenFileDialog { Filter = "Пакет PlayStation 4 (*.pkg)|*.pkg", Title = "Установить игру, обновление или DLC" };
        if (dialog.ShowDialog(this) != DialogResult.OK) return;
        busy = true;
        cancelPath = Path.Combine(AppContext.BaseDirectory, ".cancel-" + Guid.NewGuid().ToString("N"));
        try
        {
            progress.Style = ProgressBarStyle.Marquee; status.Text = "Проверка пакета…";
            var info = await RunWorker("probe", dialog.FileName);
            if (File.Exists(cancelPath)) return;
            string category = info.GetProperty("category").GetString()!;
            string kind = category == "gp" ? "Обновление" : category == "ac" ? "DLC" : "Игра";
            var question = $"{kind}: {info.GetProperty("title").GetString()}\n{info.GetProperty("titleId").GetString()} · {info.GetProperty("version").GetString()}\nРаспакованный размер: {info.GetProperty("bytes").GetInt64() / 1073741824.0:F2} ГБ\n\nУстановить?";
            if (MessageBox.Show(this, question, "Установка PKG", MessageBoxButtons.YesNo, MessageBoxIcon.Question) != DialogResult.Yes) return;
            progress.Style = ProgressBarStyle.Continuous;
            var result = await RunWorker("install", dialog.FileName);
            await RefreshLibrary(); status.Text = "Установлено: " + result.GetProperty("title").GetString();
        }
        catch (Exception ex) { status.Text = "Установка не завершена."; Error(ex); }
        finally
        {
            worker = null; busy = false; progress.Style = ProgressBarStyle.Continuous; progress.Value = 0;
            if (cancelPath != null && File.Exists(cancelPath)) File.Delete(cancelPath);
            cancelPath = null;
        }
    }

    void Cancel()
    {
        if (busy && cancelPath != null) { File.WriteAllText(cancelPath, "cancel"); status.Text = "Отмена установки…"; }
    }

    [StructLayout(LayoutKind.Sequential)] struct PadState { public uint Packet; public ushort Buttons; public byte LT, RT; public short LX, LY, RX, RY; }
    [DllImport("xinput9_1_0.dll")] static extern uint XInputGetState(uint index, out PadState state);
    void PollPad()
    {
        if (Form.ActiveForm != this) { previousButtons = 0; return; }
        PadState state = default; bool found = false;
        for (uint i = 0; i < 4; ++i) if (XInputGetState(i, out state) == 0) { found = true; break; }
        if (!found) { previousButtons = 0; return; }
        ushort pressed = (ushort)(state.Buttons & ~previousButtons); previousButtons = state.Buttons;
        if ((pressed & 0x1000) != 0) { if (toolbar.ContainsFocus) (ActiveControl as Button)?.PerformClick(); else Launch(); }
        if ((pressed & 0x2000) != 0) { Cancel(); library.Focus(); }
        if ((pressed & 0x4000) != 0) _ = Install();
        if ((pressed & 0x8000) != 0) _ = RefreshLibrary();
        if ((pressed & 0x300) != 0) { SelectNextControl(ActiveControl, (pressed & 0x200) != 0, true, true, true); return; }
        int delta = (pressed & 9) != 0 ? -1 : (pressed & 6) != 0 ? 1 : 0;
        if (delta != 0 && library.Items.Count > 0)
        {
            int index = library.SelectedIndices.Count > 0 ? library.SelectedIndices[0] : 0;
            library.Items[index].Selected = false; index = Math.Clamp(index + delta, 0, library.Items.Count - 1);
            library.Items[index].Selected = true; library.Items[index].EnsureVisible(); library.Focus();
        }
    }
}

static class Program
{
    [STAThread] static int Main(string[] args)
    {
        if (args.Contains("--external-core-folder-test")) return ExternalCoreTests.Run();
        ApplicationConfiguration.Initialize();
        try
        {
            if (args.Contains("--core-versions-test")) return CoreVersionTests.Run();
            if (args.Contains("--list-cores"))
            {
                File.WriteAllText(Path.Combine(Settings.DataRoot, "core-list.json"), JsonSerializer.Serialize(CoreVersions.Discover()));
                return 0;
            }
            int launchIndex = Array.IndexOf(args, "--launch-game");
            if (launchIndex >= 0)
            {
                var launchSettings = Settings.Load();
                int captureIndex = Array.IndexOf(args, "--capture-root");
                string? captureRoot = captureIndex >= 0 ? args[captureIndex + 1] : null;
                var start = NativeGameLaunch.Create(launchSettings, args[launchIndex + 1], captureRoot);
                using var process = new Process { StartInfo = start };
                if (!NativeGameLaunch.Start(process)) throw new IOException("Не удалось запустить ядро");
                if (captureRoot is not null)
                {
                    var output = process.StandardOutput.ReadToEndAsync();
                    var errors = process.StandardError.ReadToEndAsync();
                    bool runtimeVerified = false;
                    try
                    {
                        NativeGameLaunch.Record(launchSettings, process, captureRoot);
                        var runtime = NativeGameLaunch.ObserveRuntime(process, captureRoot);
                        process.WaitForExit(); runtimeVerified = runtime.GetAwaiter().GetResult();
                    }
                    finally
                    {
                        if (!process.HasExited) process.Kill(entireProcessTree: true);
                        process.WaitForExit();
                        File.WriteAllText(Path.Combine(captureRoot, "stdout.txt"), output.GetAwaiter().GetResult());
                        File.WriteAllText(Path.Combine(captureRoot, "stderr.txt"), errors.GetAwaiter().GetResult());
                        File.WriteAllText(Path.Combine(captureRoot, "exit.json"), JsonSerializer.Serialize(new { exitCode = process.ExitCode, runtimeVerified }));
                    }
                    return process.ExitCode != 0 ? process.ExitCode : runtimeVerified ? 0 : 1;
                }
                NativeGameLaunch.Record(launchSettings, process, Settings.DataRoot);
                return 0;
            }
            if (args.Contains("--prepare-native-profile"))
            {
                NativeProfile.Ensure(Settings.Load());
                return 0;
            }
            if (args.Contains("--native-probe-test"))
            {
                var connected = NativeConnection.Probe(Settings.Load()).GetAwaiter().GetResult();
                File.WriteAllText(Path.Combine(AppContext.BaseDirectory, "native-connection-test.json"), JsonSerializer.Serialize(connected));
                return 0;
            }
            if (args.Contains("--self-test"))
            {
                var settings = Settings.Load();
                var games = Library.Scan(settings.LibraryRoots.Append(settings.InstallRoot));
                if (!File.Exists(settings.NativeCore) ||
                    (!File.Exists(Path.Combine(AppContext.BaseDirectory, "installer", "PS4PackageInstaller.exe")) &&
                     (!File.Exists(settings.Python) || !File.Exists(Path.Combine(AppContext.BaseDirectory, "installer", "pkg_worker.py"))))) return 2;
                // Construct and dispose all menu controls without launching a game.
                using var menu = new MainWindow(settings);
                if(games.Count > 0 && settings.Backend == "dx12" && !menu.ExperimentalLaunchEnabled) return 3;
                File.WriteAllText(Path.Combine(AppContext.BaseDirectory, "self-test.json"), JsonSerializer.Serialize(games, new JsonSerializerOptions { WriteIndented = true }));
                return 0;
            }
            using var mutex = new Mutex(true, "Local\\PS4DesktopLibrary", out bool first);
            if (!first) return 0;
            var value = Settings.Load(); value.Save(); Application.Run(new MainWindow(value)); return 0;
        }
        catch (Exception ex) { MessageBox.Show(ex.Message, "PS4 — ошибка запуска"); return 1; }
    }
}
