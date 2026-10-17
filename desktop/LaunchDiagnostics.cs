using System.Diagnostics;
using System.Text.Json;

namespace PS4Desktop;

sealed class LaunchDiagnostics
{
    readonly Process process;
    readonly CancellationTokenSource pipes = new();
    readonly Task<string?> output, errors;
    public string DirectoryPath { get; }
    public string ReportPath => Path.Combine(DirectoryPath, "exit-report.json");

    public LaunchDiagnostics(Process process, string root)
    {
        this.process = process;
        DirectoryPath = Path.Combine(root, "launch-diagnostics",
            DateTime.UtcNow.ToString("yyyyMMddTHHmmssfffZ") + "-" + process.Id + "-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(DirectoryPath);
        output = Copy(process.StandardOutput.BaseStream, "stdout.txt");
        errors = Copy(process.StandardError.BaseStream, "stderr.txt");
    }

    async Task<string?> Copy(Stream source, string name)
    {
        try
        {
            await using var file = new FileStream(Path.Combine(DirectoryPath, name), FileMode.CreateNew,
                FileAccess.Write, FileShare.Read, 65536, FileOptions.Asynchronous | FileOptions.WriteThrough);
            try { await source.CopyToAsync(file, pipes.Token); }
            finally { await file.FlushAsync(); file.Flush(flushToDisk: true); }
            return null;
        }
        catch (Exception error) when (error is IOException or OperationCanceledException or ObjectDisposedException)
        { return name + ": " + error.Message; }
    }

    public void PreserveManifest(string manifest)
    {
        File.Copy(manifest, Path.Combine(DirectoryPath, "launch-manifest.json"), overwrite: false);
    }

    public async Task<int> CompleteAsync()
    {
        await process.WaitForExitAsync();
        int code = process.ExitCode;
        // A surviving child must not keep the UI waiting on inherited pipes.
        pipes.CancelAfter(TimeSpan.FromSeconds(5));
        string?[] captureErrors = await Task.WhenAll(output, errors);
        pipes.Dispose();
        var logErrors = new List<string>();
        foreach (string name in new[] { "shad_log.txt", "shadps4.log" })
        {
            string source = Path.Combine(process.StartInfo.WorkingDirectory, "user", "log", name);
            try
            {
                if (!File.Exists(source)) continue;
                using var input = new FileStream(source, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
                using var saved = new FileStream(Path.Combine(DirectoryPath, name), FileMode.CreateNew);
                input.CopyTo(saved); saved.Flush(flushToDisk: true);
            }
            catch (Exception error) when (error is IOException or UnauthorizedAccessException)
            { logErrors.Add(name + ": " + error.Message); }
        }
        string json = JsonSerializer.Serialize(new { version = 1, exitCode = code,
            exitCodeHex = $"0x{unchecked((uint)code):X8}", completedUtc = DateTimeOffset.UtcNow,
            stdout = "stdout.txt", stderr = "stderr.txt", captureErrors, logErrors }, new JsonSerializerOptions { WriteIndented = true });
        using var report = new FileStream(ReportPath, FileMode.CreateNew, FileAccess.Write, FileShare.Read);
        byte[] bytes = System.Text.Encoding.UTF8.GetBytes(json); report.Write(bytes); report.Flush(flushToDisk: true);
        return code;
    }

    public static string FailureMessage(int code, string report) =>
        $"Ядро аварийно завершило работу.\nКод: {code} (0x{unchecked((uint)code):X8}).\nДиагностический отчёт: {report}";
}
