using System.Diagnostics;
using System.Reflection;
using System.Text.Json;

namespace PS4Desktop;

static class LaunchDiagnosticsTests
{
    public static int Run()
    {
        try
        {
            string root = Path.Combine(Path.GetTempPath(), "ps4-launch-diagnostics-test-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(root);
            string manifest = Path.Combine(root, "launch-manifest.json");
            File.WriteAllText(manifest, "{\"testIdentity\":\"preserved\"}");
            string? firstReport = null;
            foreach (int expectedCode in new[] { -2147483645, 0 })
            {
                var start = new ProcessStartInfo(Environment.ProcessPath!) { UseShellExecute = false,
                    CreateNoWindow = true, WorkingDirectory = root, RedirectStandardOutput = true, RedirectStandardError = true };
                if (Path.GetFileNameWithoutExtension(start.FileName).Equals("dotnet", StringComparison.OrdinalIgnoreCase))
                    start.ArgumentList.Add(Assembly.GetExecutingAssembly().Location);
                start.ArgumentList.Add("--launch-diagnostics-child"); start.ArgumentList.Add(expectedCode.ToString());
                using var process = Process.Start(start) ?? throw new IOException("Diagnostic child did not start");
                var diagnostics = new LaunchDiagnostics(process, root); diagnostics.PreserveManifest(manifest);
                int code = diagnostics.CompleteAsync().GetAwaiter().GetResult();
                if (code != expectedCode || File.ReadAllText(Path.Combine(diagnostics.DirectoryPath, "stdout.txt")) != "stdout-without-newline" ||
                    File.ReadAllText(Path.Combine(diagnostics.DirectoryPath, "stderr.txt")) != "fatal-without-newline")
                    throw new IOException("Exit identity or final unbuffered stream bytes were lost");
                using var report = JsonDocument.Parse(File.ReadAllText(diagnostics.ReportPath));
                if (report.RootElement.GetProperty("exitCodeHex").GetString() != $"0x{unchecked((uint)expectedCode):X8}" ||
                    File.ReadAllText(Path.Combine(diagnostics.DirectoryPath, "launch-manifest.json")) != File.ReadAllText(manifest))
                    throw new IOException("Report lost hexadecimal exit code or the original launch identity");
                if (expectedCode != 0 && !LaunchDiagnostics.FailureMessage(code, diagnostics.ReportPath).Contains("0x80000003"))
                    throw new IOException("User message omitted the actual exception code");
                if (firstReport is not null && (!File.Exists(firstReport) || firstReport == diagnostics.ReportPath))
                    throw new IOException("A later launch overwrote the preceding diagnostic report");
                firstReport = diagnostics.ReportPath;
            }
            Console.WriteLine("PASS: stdout/stderr tails, signed/hex exit identity, immutable manifests and per-launch reports; " + root);
            return 0;
        }
        catch (Exception error) { Console.Error.WriteLine(error); return 1; }
    }
}
