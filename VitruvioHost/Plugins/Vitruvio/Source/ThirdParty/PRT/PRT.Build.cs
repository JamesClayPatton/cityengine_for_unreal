/* Copyright 2024 Esri
 *
 * Licensed under the Apache License Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

using System.IO;
using System;
using System.Diagnostics;
using System.Security.Cryptography;
using UnrealBuildTool;
using System.ComponentModel.Design;
using System.Collections.Generic;
using System.Linq;
using System.Text.RegularExpressions;
using EpicGames.Core;

public class PRT : ModuleRules
{
	private readonly bool Debug;


	private static readonly object PrtInstallLock = new object();

	private static readonly List<string> FilteredExtensionLibraries = new List<string>() { "DatasmithSDK.dll", "FreeImage317.dll", "com.esri.prt.unreal.dll" };

	public PRT(ReadOnlyTargetRules Target) : base(Target)
	{
		// Debug print only enabled when plugin is installed directly into project (not in Engine)
		Debug = !PluginDirectory.EndsWith(Path.Combine("Plugins", "Marketplace", "Vitruvio"));
		
		bUseRTTI = true;
		bEnableExceptions = true;
		Type = ModuleType.External;

		AbstractPlatform Platform;
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			Platform = new WindowsPlatform(Debug);
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			Platform = new LinuxPlatform(Debug);
		}
		else
		{
			throw new System.PlatformNotSupportedException();
		}

		string VersionPath = Path.Combine(ModuleDirectory, "PRT.version.json");
		ExternalDependencies.Add(VersionPath);
		JsonObject Version = JsonObject.Read(new FileReference(VersionPath));
		// The Linux SDK has its own toolchain and digest in the "linux" section, the rest of the metadata is shared
		JsonObject PlatformVersion = Version;
		if (Platform.MetadataSection != null && !Version.TryGetObjectField(Platform.MetadataSection, out PlatformVersion))
		{
			throw new BuildException($"Invalid PRT SDK metadata in {VersionPath}: missing \"{Platform.MetadataSection}\" section.");
		}
		if (!Version.TryGetIntegerField("major", out int PrtMajor) || PrtMajor <= 0 ||
			!Version.TryGetIntegerField("minor", out int PrtMinor) || PrtMinor < 0 ||
			!Version.TryGetIntegerField("build", out int PrtBuild) || PrtBuild <= 0 ||
			!PlatformVersion.TryGetStringField("toolchain", out string PrtToolchain) ||
			!Regex.IsMatch(PrtToolchain, Platform.ToolchainPattern) ||
			!PlatformVersion.TryGetStringField("sha256", out string PrtSha256) ||
			!Regex.IsMatch(PrtSha256, @"^[0-9a-fA-F]{64}$"))
		{
			throw new BuildException($"Invalid PRT SDK metadata in {VersionPath}: expected major/minor/build, a {Platform.Name} x64 release toolchain, and a SHA-256 digest.");
		}

		string LibDir = Path.Combine(ModuleDirectory, "lib", Platform.Name, "Release");
		string BinDir = Path.Combine(ModuleDirectory, "bin", Platform.Name, "Release");
		string IncludeDir = Path.Combine(ModuleDirectory, "include");

		// Build rules of several targets (e.g. editor and Linux game) can be created in parallel and share the include folder
		lock (PrtInstallLock)
		{
			// 1. Check if prt is already available and has correct version, otherwise download from official github repo
			// The include folder is shared between platforms and is removed when downloading PRT for another platform
			bool PrtInstalled = Directory.Exists(LibDir) && Directory.Exists(BinDir) && File.Exists(Path.Combine(IncludeDir, "prt", "API.h"));
		
			string PrtCorePath = Path.Combine(BinDir, Platform.CoreLibraryName);
			bool PrtCoreExists = File.Exists(PrtCorePath);
			bool PrtVersionMatch = PrtCoreExists && CheckDllVersion(Platform, PrtCorePath, PrtMajor, PrtMinor, PrtBuild);

			if (!PrtInstalled || !PrtVersionMatch)
			{

				string PrtUrl = "https://github.com/Esri/esri-cityengine-sdk/releases/download";
				string PrtVersion = string.Format("{0}.{1}.{2}", PrtMajor, PrtMinor, PrtBuild);

				string PrtLibName = string.Format("esri_ce_sdk-{0}-{1}", PrtVersion, PrtToolchain);
				string PrtLibZipFile = PrtLibName + ".zip";
				string PrtLibZipPath = Path.Combine(ModuleDirectory, PrtLibZipFile);
				string PrtDownloadUrl = Path.Combine(PrtUrl, PrtVersion, PrtLibZipFile);

				try
				{
					if (Debug)
					{
						if (!PrtInstalled) Console.WriteLine("PRT not found");
						Console.WriteLine("Updating PRT");
					}

					if (Debug) System.Console.WriteLine("Downloading " + PrtDownloadUrl + "...");
				
					Platform.DownloadFile(PrtDownloadUrl, PrtLibZipPath);

					string ActualSha256;
					using (FileStream Archive = File.OpenRead(PrtLibZipPath))
					using (SHA256 Hasher = SHA256.Create())
					{
						ActualSha256 = Convert.ToHexString(Hasher.ComputeHash(Archive));
					}

					if (!string.Equals(ActualSha256, PrtSha256, StringComparison.OrdinalIgnoreCase))
					{
						throw new BuildException($"SHA-256 mismatch for PRT SDK '{PrtLibZipFile}': expected {PrtSha256}, got {ActualSha256}.");
					}

					if (Directory.Exists(LibDir)) Directory.Delete(LibDir, true);
					if (Directory.Exists(BinDir)) Directory.Delete(BinDir, true);
					if (Directory.Exists(IncludeDir)) Directory.Delete(IncludeDir, true);

					if (Debug) System.Console.WriteLine("Extracting " + PrtLibZipFile + "...");

					Platform.ZipExtractor.Unzip(ModuleDirectory, PrtLibZipFile, PrtLibName);

					Directory.CreateDirectory(LibDir);
					Directory.CreateDirectory(BinDir);
					Copy(Path.Combine(ModuleDirectory, PrtLibName, "lib"), Path.Combine(ModuleDirectory, LibDir), FilteredExtensionLibraries);
					Copy(Path.Combine(ModuleDirectory, PrtLibName, "bin"), Path.Combine(ModuleDirectory, BinDir));
					Copy(Path.Combine(ModuleDirectory, PrtLibName, "include"), Path.Combine(ModuleDirectory, "include"));

					string VersionFile = Path.Combine(ModuleDirectory, PrtLibName, "cmake", "prtVersion.properties");
					if (File.Exists(VersionFile)) File.Copy(VersionFile, Path.Combine(BinDir, "prtVersion.properties"));
				}
				finally
				{
					File.Delete(PrtLibZipPath);
					string ExtractedDirectory = Path.Combine(ModuleDirectory, PrtLibName);
					if (Directory.Exists(ExtractedDirectory)) Directory.Delete(ExtractedDirectory, true);
				}
			}
			else if (Debug)
			{
				Console.WriteLine("PRT found");
			}
		}
		
		// 2. Copy libraries to module binaries directory and add dependencies
		string ModuleBinariesDir = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../..", "Binaries", Platform.Name));

		if (Debug)
		{
			System.Console.WriteLine("PRT Source Lib Dir: " + LibDir);
			System.Console.WriteLine("PRT Source Bin Dir: " + BinDir);
			System.Console.WriteLine("PRT Source Include Dir: " + IncludeDir);
			System.Console.WriteLine("Module Binaries Dir: " + ModuleBinariesDir);
		}

		Directory.CreateDirectory(ModuleBinariesDir);
		PublicRuntimeLibraryPaths.Add(ModuleBinariesDir);

		// Add PRT core libraries
		if (Debug) Console.WriteLine("Adding PRT core libraries");
		foreach (string FilePath in Directory.GetFiles(BinDir))
		{
			string LibraryName = Path.GetFileName(FilePath);

			Platform.AddPrtCoreLibrary(FilePath, LibraryName, this);
		}
		
		// Add extension libraries as run-time dependencies
		if (Debug) Console.WriteLine("Adding PRT extension libraries");
		Platform.AddExtensionLibraries(LibDir, this);

		// Add include search path
		if (Debug) Console.WriteLine("Adding include search path " + IncludeDir);
		PublicSystemIncludePaths.Add(IncludeDir);
		
		// Add prt version defines
		PublicDefinitions.Add($"PRT_VERSION_MAJOR={PrtMajor}");
		PublicDefinitions.Add($"PRT_VERSION_MINOR={PrtMinor}");
	}

	void Copy(string SrcDir, string DstDir, List<string> Filter = null)
	{
		Directory.CreateDirectory(DstDir);

		foreach (string CopyFile in Directory.GetFiles(SrcDir))
		{
			if (Filter == null || !Filter.Contains(Path.GetFileName(CopyFile)))
			{
				File.Copy(CopyFile, Path.Combine(DstDir, Path.GetFileName(CopyFile)));
			}
		}

		foreach (string Dir in Directory.GetDirectories(SrcDir))
		{
			Copy(Dir, Path.Combine(DstDir, Path.GetFileName(Dir)));
		}
	}

	private bool CheckDllVersion(AbstractPlatform Platform, string DllPath, int Major, int Minor, int Build)
	{
		string FileVersion = Platform.GetFileVersionInfo(ModuleDirectory, DllPath);
		string[] BuildVersions = FileVersion.Split(' ');
		string ProductVersion = BuildVersions[0];
		string[] ProductVersions = ProductVersion.Split('.');
		int FileMajor = int.Parse(ProductVersions[0]);
		int FileMinor = int.Parse(ProductVersions[1]);
		int DllBuild = int.Parse(BuildVersions[BuildVersions.Length - 1]);
		
		bool Match = FileMajor == Major && FileMinor == Minor && DllBuild == Build;
		if (Debug && !Match)
		{
			Console.WriteLine(string.Format("Version {0}.{1}.{2} of \"{3}\" does not match expected version of Build file {4}.{5}.{6}",
				FileMajor, FileMinor, DllBuild, Path.GetFileName(DllPath), Major, Minor, Build));
		}
		return Match;
	}

	private abstract class AbstractZipExtractor
	{
		public void Unzip(string WorkingDir, string ZipFile, string Destination)
		{
			string ExpandedArguments = string.Format(Arguments, ZipFile, Destination);

			ProcessStartInfo ProcStartInfo = new System.Diagnostics.ProcessStartInfo(Command, ExpandedArguments)
			{
				WorkingDirectory = WorkingDir,
				UseShellExecute = false,
				CreateNoWindow = true
			};

			System.Diagnostics.Process UnzipProcess = new System.Diagnostics.Process
			{
				StartInfo = ProcStartInfo,
				EnableRaisingEvents = true
			};
			UnzipProcess.Start();
			UnzipProcess.WaitForExit();

			if (UnzipProcess.ExitCode != 0)
			{
				throw new BuildException("Failed to extract {0} (exit code {1}), check that the PRT download succeeded", ZipFile, UnzipProcess.ExitCode);
			}
		}

		public abstract string Command { get; }
		public abstract string Arguments { get; }
	}

	private class WindowsZipExtractor : AbstractZipExtractor
	{
		public override string Command { get { return "cmd"; } }

		public override string Arguments
		{
			get
			{
				return "/c PowerShell -Command \" & Expand-Archive -Path {0} -DestinationPath {1}\"";
			}
		}
	}

	private class UnixZipExtractor : AbstractZipExtractor
	{
		public override string Command { get { return "unzip"; } }
		public override string Arguments { get { return "-q {0} -d {1}"; } }
	}

	private abstract class AbstractPlatform
	{
		public abstract AbstractZipExtractor ZipExtractor { get; }

		public abstract string Name { get; }
		public abstract string DynamicLibExtension { get; }
		public virtual string CoreLibraryName { get { return "com.esri.prt.core" + DynamicLibExtension; } }
		// Section of PRT.version.json with the toolchain and digest of this platform, null for the top level
		public virtual string MetadataSection { get { return null; } }
		public abstract string ToolchainPattern { get; }

		protected bool Debug;
		public AbstractPlatform(bool Debug)
		{
			this.Debug = Debug;
		}

		public virtual void AddExtensionLibraries(string SourceFolder, ModuleRules Rules)
		{
			foreach (string Dir in Directory.GetDirectories(SourceFolder))
			{
				AddExtensionLibraries(Dir, Rules);
			}
			
			foreach (string FilePath in Directory.GetFiles(SourceFolder))
			{
				Rules.RuntimeDependencies.Add(FilePath);
			}
		}

		public virtual void AddPrtCoreLibrary(string LibraryPath, string LibraryName, ModuleRules Rules)
		{
			if (Path.GetExtension(LibraryName) == DynamicLibExtension)
			{
				if (Debug) Console.WriteLine("Adding Runtime Library " + LibraryName);

				Rules.RuntimeDependencies.Add(LibraryPath);
				Rules.PublicDelayLoadDLLs.Add(LibraryName);
			}
		}
		public abstract string GetFileVersionInfo(string WorkingDir, string Path);
		public abstract void DownloadFile(string Url, string Destination);
	}

	private class WindowsPlatform : AbstractPlatform
	{
		public override AbstractZipExtractor ZipExtractor { get { return new WindowsZipExtractor(); } }

		public override string Name { get { return "Win64"; } }
		public override string DynamicLibExtension { get { return ".dll"; } }
		public override string ToolchainPattern { get { return @"^win[0-9]+-vc[0-9]{4}-x86_64-rel-opt$"; } }
		
		public WindowsPlatform(bool Debug) : base(Debug)
		{
		}
		
		public override void AddPrtCoreLibrary(string LibraryPath, string LibraryName, ModuleRules Rules)
		{
			base.AddPrtCoreLibrary(LibraryPath, LibraryName, Rules);

			if (Path.GetExtension(LibraryPath) == ".lib")
			{
				if (Debug) Console.WriteLine("Adding Public Additional Library " + LibraryName);

				Rules.PublicAdditionalLibraries.Add(LibraryPath);
			}
		}
		

		public override string GetFileVersionInfo(string WorkingDir, string Path)
		{
			string GetFileInfoCommand = "/c PowerShell -Command \"(Get-Item \'{0}\').VersionInfo.FileVersion\"";
			string ExpandedFileInfoCommand = string.Format(GetFileInfoCommand, Path);

			ProcessStartInfo ProcStartInfo = new System.Diagnostics.ProcessStartInfo("cmd", ExpandedFileInfoCommand)
			{
				WorkingDirectory = WorkingDir,
				UseShellExecute = false,
				CreateNoWindow = true,
				RedirectStandardOutput = true
			};

			Process FileVersionProcess = new Process
			{
				StartInfo = ProcStartInfo,
				EnableRaisingEvents = true
			};
			FileVersionProcess.Start();
			
			string Output = FileVersionProcess.StandardOutput.ReadToEnd();
			FileVersionProcess.WaitForExit();

			return Output;
		}
		
		public override void DownloadFile(string Url, string Destination)
		{
			string DownloadFileCommand = "/c PowerShell -Command \"Invoke-WebRequest -Uri {0} -OutFile '{1}'\"";
			string ExpandedDownloadFileCommand = string.Format(DownloadFileCommand, Url, Destination);

			ProcessStartInfo ProcStartInfo = new System.Diagnostics.ProcessStartInfo("cmd", ExpandedDownloadFileCommand)
			{
				UseShellExecute = false,
				CreateNoWindow = true,
			};

			Process FileVersionProcess = new Process
			{
				StartInfo = ProcStartInfo,
				EnableRaisingEvents = true
			};
			FileVersionProcess.Start();
			FileVersionProcess.WaitForExit();
		}
	}

	private class LinuxPlatform : AbstractPlatform
	{
		// Download and extraction run on the build host, which is Windows when cross-compiling
		public override AbstractZipExtractor ZipExtractor
		{
			get { return OperatingSystem.IsWindows() ? new WindowsZipExtractor() : new UnixZipExtractor(); }
		}

		public override string Name { get { return "Linux"; } }
		public override string DynamicLibExtension { get { return ".so"; } }
		public override string MetadataSection { get { return "linux"; } }
		public override string ToolchainPattern { get { return @"^rhel[0-9]+-gcc[0-9]+-x86_64-rel-opt$"; } }
		public override string CoreLibraryName { get { return "lib" + base.CoreLibraryName; } }

		public LinuxPlatform(bool Debug) : base(Debug)
		{
		}

		// PRT is not linked on Linux but loaded at runtime (see PrtLinuxLoader.h), so it only needs to be staged
		public override void AddPrtCoreLibrary(string LibraryPath, string LibraryName, ModuleRules Rules)
		{
			if (Path.GetExtension(LibraryName) == DynamicLibExtension)
			{
				if (Debug) Console.WriteLine("Adding Runtime Library " + LibraryName);

				Rules.RuntimeDependencies.Add(LibraryPath);
			}
		}

		// .so files carry no version resource, so read the version file copied from the SDK instead
		public override string GetFileVersionInfo(string WorkingDir, string LibraryPath)
		{
			string VersionFile = Path.Combine(Path.GetDirectoryName(LibraryPath), "prtVersion.properties");
			if (!File.Exists(VersionFile))
			{
				return "0.0 0";
			}

			Dictionary<string, string> Properties = File.ReadAllLines(VersionFile)
				.Where(Line => Line.Contains('='))
				.ToDictionary(Line => Line.Split('=')[0].Trim(), Line => Line.Split('=')[1].Trim());
			return string.Format("{0}.{1} {2}", Properties["PRT_VERSION_MAJOR"], Properties["PRT_VERSION_MINOR"], Properties["PRT_VERSION_MICRO"]);
		}

		public override void DownloadFile(string Url, string Destination)
		{
			if (OperatingSystem.IsWindows())
			{
				new WindowsPlatform(Debug).DownloadFile(Url, Destination);
				return;
			}

			ProcessStartInfo ProcStartInfo = new ProcessStartInfo("curl", string.Format("-fsSL -o \"{0}\" {1}", Destination, Url))
			{
				UseShellExecute = false,
				CreateNoWindow = true,
			};

			Process DownloadProcess = new Process
			{
				StartInfo = ProcStartInfo,
				EnableRaisingEvents = true
			};
			DownloadProcess.Start();
			DownloadProcess.WaitForExit();

			if (DownloadProcess.ExitCode != 0)
			{
				throw new BuildException("Failed to download {0} (curl exit code {1})", Url, DownloadProcess.ExitCode);
			}
		}
	}
}
