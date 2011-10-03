// protectprocess.c
// Copyright (C) 2011 Suguru Kawamoto
// �v���Z�X�̕ی�

// ���̒�����1�̂ݗL���ɂ���
// �t�b�N��̊֐��̃R�[�h������������
// �S�Ă̌Ăяo�����t�b�N�\���������I�ɓ�d�Ăяo���ɑΉ��ł��Ȃ�
#define USE_CODE_HOOK
// �t�b�N��̊֐��̃C���|�[�g�A�h���X�e�[�u��������������
// ��d�Ăяo�����\�����Ăяo�����@�ɂ���Ă̓t�b�N����������
//#define USE_IAT_HOOK

// �t�b�N�Ώۂ̊֐��� %s
// �t�b�N�Ώۂ̌^ _%s
// �t�b�N�Ώۂ̃|�C���^ p_%s
// �t�b�N�p�̊֐��� h_%s
// �t�b�N�Ώۂ̃R�[�h�̃o�b�N�A�b�v c_%s

#include <tchar.h>
#include <windows.h>
#include <ntsecapi.h>
#include <wincrypt.h>
#include <wintrust.h>
#include <softpub.h>
#include <aclapi.h>
#include <sfc.h>
#include <tlhelp32.h>
#include <imagehlp.h>
#ifdef USE_IAT_HOOK
#include <dbghelp.h>
#endif

#define DO_NOT_REPLACE
#include "protectprocess.h"
#include "mbswrapper.h"

#ifdef USE_IAT_HOOK
#pragma comment(lib, "dbghelp.lib")
#endif

#ifdef USE_CODE_HOOK
#if defined(_X86_)
#define HOOK_JUMP_CODE_LENGTH 5
#elif defined(_AMD64_)
#define HOOK_JUMP_CODE_LENGTH 14
#endif
#endif

BOOL LockThreadLock();
BOOL UnlockThreadLock();
#ifdef USE_CODE_HOOK
BOOL HookFunctionInCode(void* pOriginal, void* pNew, void* pBackupCode, BOOL bRestore);
#endif
#ifdef USE_IAT_HOOK
BOOL HookFunctionInIAT(void* pOriginal, void* pNew);
#endif
HANDLE LockExistingFile(LPCWSTR Filename);
BOOL FindTrustedModuleMD5Hash(void* pHash);
BOOL VerifyFileSignature(LPCWSTR Filename);
BOOL VerifyFileSignatureInCatalog(LPCWSTR Catalog, LPCWSTR Filename);
BOOL GetSHA1HashOfModule(LPCWSTR Filename, void* pHash);
BOOL IsModuleTrusted(LPCWSTR Filename);

// �ϐ��̐錾
#ifdef USE_CODE_HOOK
#define HOOK_FUNCTION_VAR(name) _##name p_##name;BYTE c_##name[HOOK_JUMP_CODE_LENGTH * 2];
#endif
#ifdef USE_IAT_HOOK
#define HOOK_FUNCTION_VAR(name) _##name p_##name;
#endif
// �֐��|�C���^���擾
#define GET_FUNCTION(h, name) p_##name = (_##name)GetProcAddress(h, #name)
// �t�b�N�Ώۂ̃R�[�h��u�����ăt�b�N���J�n
#define SET_HOOK_FUNCTION(name) HookFunctionInCode(p_##name, h_##name, &c_##name, FALSE)
// �t�b�N�Ώۂ��Ăяo���O�ɑΏۂ̃R�[�h�𕜌�
#define START_HOOK_FUNCTION(name) HookFunctionInCode(p_##name, h_##name, &c_##name, TRUE)
// �t�b�N�Ώۂ��Ăяo������ɑΏۂ̃R�[�h��u��
#define END_HOOK_FUNCTION(name) HookFunctionInCode(p_##name, h_##name, NULL, FALSE)

HOOK_FUNCTION_VAR(LoadLibraryA)
HOOK_FUNCTION_VAR(LoadLibraryW)
HOOK_FUNCTION_VAR(LoadLibraryExA)
HOOK_FUNCTION_VAR(LoadLibraryExW)

typedef NTSTATUS (NTAPI* _LdrLoadDll)(LPCWSTR, DWORD*, UNICODE_STRING*, HMODULE*);
typedef NTSTATUS (NTAPI* _LdrGetDllHandle)(LPCWSTR, DWORD*, UNICODE_STRING*, HMODULE*);
typedef PIMAGE_NT_HEADERS (NTAPI* _RtlImageNtHeader)(PVOID);
typedef BOOL (WINAPI* _CryptCATAdminCalcHashFromFileHandle)(HANDLE, DWORD*, BYTE*, DWORD);

_LdrLoadDll p_LdrLoadDll;
_LdrGetDllHandle p_LdrGetDllHandle;
_RtlImageNtHeader p_RtlImageNtHeader;
_CryptCATAdminCalcHashFromFileHandle p_CryptCATAdminCalcHashFromFileHandle;

#define MAX_LOCKED_THREAD 16
#define MAX_TRUSTED_FILENAME_TABLE 16
#define MAX_TRUSTED_MD5_HASH_TABLE 16

DWORD g_LockedThread[MAX_LOCKED_THREAD];
WCHAR* g_pTrustedFilenameTable[MAX_TRUSTED_FILENAME_TABLE];
BYTE g_TrustedMD5HashTable[MAX_TRUSTED_MD5_HASH_TABLE][16];

// �ȉ��t�b�N�֐�
// �t�b�N�Ώۂ��Ăяo���ꍇ�͑O���START_HOOK_FUNCTION��END_HOOK_FUNCTION�����s����K�v������

HMODULE WINAPI h_LoadLibraryA(LPCSTR lpLibFileName)
{
	HMODULE r = NULL;
	wchar_t* pw0 = NULL;
	if(pw0 = DuplicateAtoW(lpLibFileName, -1))
		r = LoadLibraryExW(pw0, NULL, 0);
	FreeDuplicatedString(pw0);
	return r;
}

HMODULE WINAPI h_LoadLibraryW(LPCWSTR lpLibFileName)
{
	HMODULE r = NULL;
	r = LoadLibraryExW(lpLibFileName, NULL, 0);
	return r;
}

HMODULE WINAPI h_LoadLibraryExA(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
	HMODULE r = NULL;
	wchar_t* pw0 = NULL;
	if(pw0 = DuplicateAtoW(lpLibFileName, -1))
		r = LoadLibraryExW(pw0, hFile, dwFlags);
	FreeDuplicatedString(pw0);
	return r;
}

HMODULE WINAPI h_LoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
	HMODULE r = NULL;
	BOOL bTrusted;
	wchar_t* pw0;
	HANDLE hLock;
	HMODULE hModule;
	DWORD Length;
	bTrusted = FALSE;
	pw0 = NULL;
	hLock = NULL;
//	if(dwFlags & (DONT_RESOLVE_DLL_REFERENCES | LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE))
	if(dwFlags & (DONT_RESOLVE_DLL_REFERENCES | LOAD_LIBRARY_AS_DATAFILE | 0x00000020 | 0x00000040))
		bTrusted = TRUE;
	if(!bTrusted)
	{
		if(hModule = System_LoadLibrary(lpLibFileName, NULL, DONT_RESOLVE_DLL_REFERENCES))
		{
			Length = MAX_PATH;
			if(pw0 = AllocateStringW(Length))
			{
				if(GetModuleFileNameW(hModule, pw0, Length) > 0)
				{
					while(pw0)
					{
						if(GetModuleFileNameW(hModule, pw0, Length) + 1 <= Length)
						{
							lpLibFileName = pw0;
							break;
						}
						Length = Length * 2;
						FreeDuplicatedString(pw0);
						pw0 = AllocateStringW(Length);
					}
				}
			}
			hLock = LockExistingFile(lpLibFileName);
			FreeLibrary(hModule);
		}
		if(GetModuleHandleW(lpLibFileName))
			bTrusted = TRUE;
	}
	if(!bTrusted)
	{
		if(LockThreadLock())
		{
			if(hLock)
			{
				if(IsModuleTrusted(lpLibFileName))
					bTrusted = TRUE;
			}
			UnlockThreadLock();
		}
	}
	if(bTrusted)
		r = System_LoadLibrary(lpLibFileName, hFile, dwFlags);
	FreeDuplicatedString(pw0);
	if(hLock)
		CloseHandle(hLock);
	return r;
}

// �ȉ��w���p�[�֐�

BOOL LockThreadLock()
{
	BOOL bResult;
	DWORD ThreadId;
	DWORD i;
	bResult = FALSE;
	ThreadId = GetCurrentThreadId();
	i = 0;
	while(i < MAX_LOCKED_THREAD)
	{
		if(g_LockedThread[i] == ThreadId)
			break;
		i++;
	}
	if(i >= MAX_LOCKED_THREAD)
	{
		i = 0;
		while(i < MAX_LOCKED_THREAD)
		{
			if(g_LockedThread[i] == 0)
			{
				g_LockedThread[i] = ThreadId;
				bResult = TRUE;
				break;
			}
			i++;
		}
	}
	return bResult;
}

BOOL UnlockThreadLock()
{
	BOOL bResult;
	DWORD ThreadId;
	DWORD i;
	bResult = FALSE;
	ThreadId = GetCurrentThreadId();
	i = 0;
	while(i < MAX_LOCKED_THREAD)
	{
		if(g_LockedThread[i] == ThreadId)
		{
			g_LockedThread[i] = 0;
			bResult = TRUE;
			break;
		}
		i++;
	}
	return bResult;
}

#ifdef USE_CODE_HOOK
BOOL HookFunctionInCode(void* pOriginal, void* pNew, void* pBackupCode, BOOL bRestore)
{
	BOOL bResult;
	DWORD Protect;
#if defined(_X86_)
	BYTE JumpCode[HOOK_JUMP_CODE_LENGTH] = {0xe9, 0x00, 0x00, 0x00, 0x00};
	size_t Relative;
	Relative = (size_t)pNew - (size_t)pOriginal - HOOK_JUMP_CODE_LENGTH;
	memcpy(&JumpCode[1], &Relative, 4);
	bResult = FALSE;
	if(bRestore)
	{
		if(VirtualProtect(pOriginal, HOOK_JUMP_CODE_LENGTH, PAGE_EXECUTE_READWRITE, &Protect))
		{
			memcpy(pOriginal, pBackupCode, HOOK_JUMP_CODE_LENGTH);
			VirtualProtect(pOriginal, HOOK_JUMP_CODE_LENGTH, Protect, &Protect);
			bResult = TRUE;
		}
	}
	else
	{
		if(pBackupCode)
			memcpy(pBackupCode, pOriginal, HOOK_JUMP_CODE_LENGTH);
		if(VirtualProtect(pOriginal, HOOK_JUMP_CODE_LENGTH, PAGE_EXECUTE_READWRITE, &Protect))
		{
			memcpy(pOriginal, &JumpCode, HOOK_JUMP_CODE_LENGTH);
			VirtualProtect(pOriginal, HOOK_JUMP_CODE_LENGTH, Protect, &Protect);
			bResult = TRUE;
		}
	}
#elif defined(_AMD64_)
	BYTE JumpCode[HOOK_JUMP_CODE_LENGTH] = {0xff, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	size_t Absolute;
	Absolute = (size_t)pOriginal;
	memcpy(&JumpCode[6], &Absolute, 8);
	bResult = FALSE;
	if(bRestore)
	{
		if(VirtualProtect(pOriginal, HOOK_JUMP_CODE_LENGTH, PAGE_EXECUTE_READWRITE, &Protect))
		{
			memcpy(pOriginal, pBackupCode, HOOK_JUMP_CODE_LENGTH);
			VirtualProtect(pOriginal, HOOK_JUMP_CODE_LENGTH, Protect, &Protect);
			bResult = TRUE;
		}
	}
	else
	{
		if(pBackupCode)
			memcpy(pBackupCode, pOriginal, HOOK_JUMP_CODE_LENGTH);
		if(VirtualProtect(pOriginal, HOOK_JUMP_CODE_LENGTH, PAGE_EXECUTE_READWRITE, &Protect))
		{
			memcpy(pOriginal, &JumpCode, HOOK_JUMP_CODE_LENGTH);
			VirtualProtect(pOriginal, HOOK_JUMP_CODE_LENGTH, Protect, &Protect);
			bResult = TRUE;
		}
	}
#endif
	return bResult;
}
#endif

#ifdef USE_IAT_HOOK
BOOL HookFunctionInIAT(void* pOriginal, void* pNew)
{
	BOOL bResult;
	HANDLE hSnapshot;
	MODULEENTRY32 me;
	BOOL bFound;
	IMAGE_IMPORT_DESCRIPTOR* piid;
	ULONG Size;
	IMAGE_THUNK_DATA* pitd;
	DWORD Protect;
	bResult = FALSE;
	if((hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId())) != INVALID_HANDLE_VALUE)
	{
		me.dwSize = sizeof(MODULEENTRY32);
		if(Module32First(hSnapshot, &me))
		{
			bFound = FALSE;
			do
			{
				if(piid = (IMAGE_IMPORT_DESCRIPTOR*)ImageDirectoryEntryToData(me.hModule, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &Size))
				{
					while(!bFound && piid->Name != 0)
					{
						pitd = (IMAGE_THUNK_DATA*)((BYTE*)me.hModule + piid->FirstThunk);
						while(!bFound && pitd->u1.Function != 0)
						{
							if((void*)pitd->u1.Function == pOriginal)
							{
								bFound = TRUE;
								if(VirtualProtect(&pitd->u1.Function, sizeof(void*), PAGE_EXECUTE_READWRITE, &Protect))
								{
									memcpy(&pitd->u1.Function, &pNew, sizeof(void*));
									VirtualProtect(&pitd->u1.Function, sizeof(void*), Protect, &Protect);
									bResult = TRUE;
								}
							}
							pitd++;
						}
						piid++;
					}
				}
			}
			while(!bFound && Module32Next(hSnapshot, &me));
		}
		CloseHandle(hSnapshot);
	}
	return bResult;
}
#endif

// �t�@�C����ύX�s�\�ɐݒ�
HANDLE LockExistingFile(LPCWSTR Filename)
{
	HANDLE hResult;
	hResult = NULL;
	if((hResult = CreateFileW(Filename, 0, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL)) == INVALID_HANDLE_VALUE)
		hResult = NULL;
	return hResult;
}

// DLL�̃n�b�V��������
BOOL FindTrustedModuleMD5Hash(void* pHash)
{
	BOOL bResult;
	int i;
	bResult = FALSE;
	i = 0;
	while(i < MAX_TRUSTED_MD5_HASH_TABLE)
	{
		if(memcmp(&g_TrustedMD5HashTable[i], pHash, 16) == 0)
		{
			bResult = TRUE;
			break;
		}
		i++;
	}
	return bResult;
}

// �t�@�C���̏������m�F
BOOL VerifyFileSignature(LPCWSTR Filename)
{
	BOOL bResult;
	GUID g = WINTRUST_ACTION_GENERIC_VERIFY_V2;
	WINTRUST_FILE_INFO wfi;
	WINTRUST_DATA wd;
	bResult = FALSE;
	ZeroMemory(&wfi, sizeof(WINTRUST_FILE_INFO));
	wfi.cbStruct = sizeof(WINTRUST_FILE_INFO);
	wfi.pcwszFilePath = Filename;
	ZeroMemory(&wd, sizeof(WINTRUST_DATA));
	wd.cbStruct = sizeof(WINTRUST_DATA);
	wd.dwUIChoice = WTD_UI_NONE;
	wd.dwUnionChoice = WTD_CHOICE_FILE;
	wd.pFile = &wfi;
	if(WinVerifyTrust((HWND)INVALID_HANDLE_VALUE, &g, &wd) == ERROR_SUCCESS)
		bResult = TRUE;
	return bResult;
}

// �t�@�C���̏������J�^���O�t�@�C���Ŋm�F
BOOL VerifyFileSignatureInCatalog(LPCWSTR Catalog, LPCWSTR Filename)
{
	BOOL bResult;
	GUID g = WINTRUST_ACTION_GENERIC_VERIFY_V2;
	WINTRUST_CATALOG_INFO wci;
	WINTRUST_DATA wd;
	bResult = FALSE;
	if(VerifyFileSignature(Catalog))
	{
		ZeroMemory(&wci, sizeof(WINTRUST_CATALOG_INFO));
		wci.cbStruct = sizeof(WINTRUST_CATALOG_INFO);
		wci.pcwszCatalogFilePath = Catalog;
		wci.pcwszMemberFilePath = Filename;
		if((wci.hMemberFile = CreateFileW(Filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL)) != INVALID_HANDLE_VALUE)
		{
			p_CryptCATAdminCalcHashFromFileHandle(wci.hMemberFile, &wci.cbCalculatedFileHash, NULL, 0);
			if(wci.pbCalculatedFileHash = (BYTE*)malloc(wci.cbCalculatedFileHash))
			{
				if(p_CryptCATAdminCalcHashFromFileHandle(wci.hMemberFile, &wci.cbCalculatedFileHash, wci.pbCalculatedFileHash, 0))
				{
					ZeroMemory(&wd, sizeof(WINTRUST_DATA));
					wd.cbStruct = sizeof(WINTRUST_DATA);
					wd.dwUIChoice = WTD_UI_NONE;
					wd.dwUnionChoice = WTD_CHOICE_CATALOG;
					wd.pCatalog = &wci;
					if(WinVerifyTrust((HWND)INVALID_HANDLE_VALUE, &g, &wd) == ERROR_SUCCESS)
						bResult = TRUE;
				}
				free(wci.pbCalculatedFileHash);
			}
			CloseHandle(wci.hMemberFile);
		}
	}
	return bResult;
}

BOOL WINAPI GetSHA1HashOfModule_Function(DIGEST_HANDLE refdata, PBYTE pData, DWORD dwLength)
{
	return CryptHashData(*(HCRYPTHASH*)refdata, pData, dwLength, 0);
}

// ���W���[����SHA1�n�b�V�����擾
// �}�j�t�F�X�g�t�@�C����file�v�f��hash�����͎��s�\�t�@�C���̏ꍇ��ImageGetDigestStream�ŎZ�o�����
BOOL GetSHA1HashOfModule(LPCWSTR Filename, void* pHash)
{
	BOOL bResult;
	HCRYPTPROV hProv;
	HCRYPTHASH hHash;
	HANDLE hFile;
	DWORD dw;
	bResult = FALSE;
	if(CryptAcquireContextW(&hProv, NULL, NULL, PROV_RSA_FULL, 0) || CryptAcquireContextW(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_NEWKEYSET))
	{
		if(CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash))
		{
			if((hFile = CreateFileW(Filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) != INVALID_HANDLE_VALUE)
			{
				if(ImageGetDigestStream(hFile, CERT_PE_IMAGE_DIGEST_ALL_IMPORT_INFO, GetSHA1HashOfModule_Function, (DIGEST_HANDLE)&hHash))
				{
					dw = 20;
					if(CryptGetHashParam(hHash, HP_HASHVAL, (BYTE*)pHash, &dw, 0))
						bResult = TRUE;
				}
				CloseHandle(hFile);
			}
			CryptDestroyHash(hHash);
		}
		CryptReleaseContext(hProv, 0);
	}
	return bResult;
}

BOOL IsSxsModuleTrusted_Function(LPCWSTR Catalog, LPCWSTR Manifest, LPCWSTR Module)
{
	BOOL bResult;
	HANDLE hLock0;
	HANDLE hLock1;
	BYTE Hash[20];
	int i;
	static char HexTable[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
	char HashHex[41];
	HANDLE hFile;
	DWORD Size;
	char* pData;
	DWORD dw;
	bResult = FALSE;
	if(hLock0 = LockExistingFile(Catalog))
	{
		if(hLock1 = LockExistingFile(Manifest))
		{
			if(VerifyFileSignatureInCatalog(Catalog, Manifest))
			{
				if(GetSHA1HashOfModule(Module, &Hash))
				{
					for(i = 0; i < 20; i++)
					{
						HashHex[i * 2] = HexTable[(Hash[i] >> 4) & 0x0f];
						HashHex[i * 2 + 1] = HexTable[Hash[i] & 0x0f];
					}
					HashHex[i * 2] = '\0';
					if((hFile = CreateFileW(Manifest, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL)) != INVALID_HANDLE_VALUE)
					{
						Size = GetFileSize(hFile, NULL);
						if(pData = (char*)VirtualAlloc(NULL, Size + 1, MEM_COMMIT, PAGE_READWRITE))
						{
							VirtualLock(pData, Size + 1);
							if(ReadFile(hFile, pData, Size, &dw, NULL))
							{
								pData[dw] = '\0';
								if(strstr(pData, HashHex))
									bResult = TRUE;
							}
							VirtualUnlock(pData, Size + 1);
							VirtualFree(pData, Size + 1, MEM_DECOMMIT);
						}
						CloseHandle(hFile);
					}
				}
			}
			CloseHandle(hLock1);
		}
		CloseHandle(hLock0);
	}
	return bResult;
}

// �T�C�h�o�C�T�C�hDLL���m�F
// �p�X��"%SystemRoot%\WinSxS"�ȉ���z��
// �ȉ��̃t�@�C�������݂�����̂Ƃ���
// "\xxx\yyy.dll"�A"\manifests\xxx.cat"�A"\manifests\xxx.manifest"�̃Z�b�g�iXP�̑S�Ă�DLL�AVista�ȍ~�̈ꕔ��DLL�j
// "\xxx\yyy.dll"�A"\catalogs\zzz.cat"�A"\manifests\xxx.manifest"�̃Z�b�g�iVista�ȍ~�̂قƂ�ǂ�DLL�j
// �������ꂽ�J�^���O�t�@�C����p���ă}�j�t�F�X�g�t�@�C������₂���Ă��Ȃ����Ƃ��m�F
// �n�b�V���l��	�}�j�t�F�X�g�t�@�C����file�v�f��hash�����ɋL�q����Ă�����̂�p����
// �}�j�t�F�X�g�t�@�C������SHA1�n�b�V���l��16�i���\�L�𒼐ڌ������Ă��邪�m���I�ɖ��Ȃ�
BOOL IsSxsModuleTrusted(LPCWSTR Filename)
{
	BOOL bResult;
	wchar_t* pw0;
	wchar_t* pw1;
	wchar_t* pw2;
	wchar_t* pw3;
	wchar_t* pw4;
	wchar_t* pw5;
	wchar_t* p;
	HANDLE hFind;
	WIN32_FIND_DATAW wfd;
	bResult = FALSE;
	if(pw0 = AllocateStringW(wcslen(Filename) + 1))
	{
		wcscpy(pw0, Filename);
		if(p = wcsrchr(pw0, L'\\'))
		{
			wcscpy(p, L"");
			if(p = wcsrchr(pw0, L'\\'))
			{
				p++;
				if(pw1 = AllocateStringW(wcslen(p) + 1))
				{
					wcscpy(pw1, p);
					wcscpy(p, L"");
					if(pw2 = AllocateStringW(wcslen(pw0) + wcslen(L"manifests\\") + wcslen(pw1) + wcslen(L".cat") + 1))
					{
						wcscpy(pw2, pw0);
						wcscat(pw2, L"manifests\\");
						wcscat(pw2, pw1);
						if(pw3 = AllocateStringW(wcslen(pw2) + wcslen(L".manifest") + 1))
						{
							wcscpy(pw3, pw2);
							wcscat(pw3, L".manifest");
							wcscat(pw2, L".cat");
							if(IsSxsModuleTrusted_Function(pw2, pw3, Filename))
								bResult = TRUE;
							FreeDuplicatedString(pw3);
						}
						FreeDuplicatedString(pw2);
					}
					if(!bResult)
					{
						if(pw2 = AllocateStringW(wcslen(pw0) + wcslen(L"catalogs\\") + 1))
						{
							if(pw3 = AllocateStringW(wcslen(pw0) + wcslen(L"manifests\\") + wcslen(pw1) + wcslen(L".manifest") + 1))
							{
								wcscpy(pw2, pw0);
								wcscat(pw2, L"catalogs\\");
								wcscpy(pw3, pw0);
								wcscat(pw3, L"manifests\\");
								wcscat(pw3, pw1);
								wcscat(pw3, L".manifest");
								if(pw4 = AllocateStringW(wcslen(pw2) + wcslen(L"*.cat") + 1))
								{
									wcscpy(pw4, pw2);
									wcscat(pw4, L"*.cat");
									if((hFind = FindFirstFileW(pw4, &wfd)) != INVALID_HANDLE_VALUE)
									{
										do
										{
											if(pw5 = AllocateStringW(wcslen(pw2) + wcslen(wfd.cFileName) + 1))
											{
												wcscpy(pw5, pw2);
												wcscat(pw5, wfd.cFileName);
												if(IsSxsModuleTrusted_Function(pw5, pw3, Filename))
													bResult = TRUE;
												FreeDuplicatedString(pw5);
											}
										}
										while(!bResult && FindNextFileW(hFind, &wfd));
										FindClose(hFind);
									}
									FreeDuplicatedString(pw4);
								}
								FreeDuplicatedString(pw3);
							}
							FreeDuplicatedString(pw2);
						}
					}
					FreeDuplicatedString(pw1);
				}
			}
		}
		FreeDuplicatedString(pw0);
	}
	return bResult;
}

// DLL���m�F
// �n�b�V�����o�^����Ă���AAuthenticode����������Ă���A�܂���WFP�ɂ��ی쉺�ɂ��邱�Ƃ��m�F
BOOL IsModuleTrusted(LPCWSTR Filename)
{
	BOOL bResult;
	BYTE Hash[16];
	bResult = FALSE;
	if(GetMD5HashOfFile(Filename, &Hash))
	{
		if(FindTrustedModuleMD5Hash(&Hash))
			bResult = TRUE;
	}
	if(!bResult)
	{
		if(VerifyFileSignature(Filename))
			bResult = TRUE;
	}
	if(!bResult)
	{
		if(IsSxsModuleTrusted(Filename))
			bResult = TRUE;
	}
	if(!bResult)
	{
		if(SfcIsFileProtected(NULL, Filename))
			bResult = TRUE;
	}
	return bResult;
}

// kernel32.dll��LoadLibraryExW�����̊֐�
// �h�L�������g���������ߏڍׂ͕s��
// �ꕔ�̃E�B���X�΍�\�t�g�iAvast!���j��LdrLoadDll���t�b�N���Ă��邽��LdrLoadDll������������ׂ��ł͂Ȃ�
// �J�[�l�����[�h�̃R�[�h�ɑ΂��Ă͌��ʂȂ�
// SeDebugPrivilege���g�p�\�ȃ��[�U�[�ɑ΂��Ă͌��ʂȂ�
HMODULE System_LoadLibrary(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
	HMODULE r = NULL;
	UNICODE_STRING us;
	HANDLE hDataFile;
	HANDLE hMapping;
	DWORD DllFlags;
	us.Length = sizeof(wchar_t) * wcslen(lpLibFileName);
	us.MaximumLength = sizeof(wchar_t) * (wcslen(lpLibFileName) + 1);
	us.Buffer = (PWSTR)lpLibFileName;
//	if(dwFlags & (LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE))
	if(dwFlags & (LOAD_LIBRARY_AS_DATAFILE | 0x00000040))
	{
//		if(p_LdrGetDllHandle(NULL, NULL, &us, &r) == STATUS_SUCCESS)
		if(p_LdrGetDllHandle(NULL, NULL, &us, &r) == 0)
		{
//			dwFlags &= ~(LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE);
			dwFlags &= ~(LOAD_LIBRARY_AS_DATAFILE | 0x00000040);
			dwFlags |= DONT_RESOLVE_DLL_REFERENCES;
		}
		else
		{
//			if(dwFlags & LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE)
			if(dwFlags & 0x00000040)
				hDataFile = CreateFileW(lpLibFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
			else
				hDataFile = CreateFileW(lpLibFileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
			if(hDataFile != INVALID_HANDLE_VALUE)
			{
				if(hMapping = CreateFileMappingW(hDataFile, NULL, PAGE_READONLY, 0, 0, NULL))
				{
					if(r = (HMODULE)MapViewOfFileEx(hMapping, FILE_MAP_READ, 0, 0, 0, NULL))
					{
						if(p_RtlImageNtHeader(r))
							r = (HMODULE)((size_t)r | 1);
						else
						{
							UnmapViewOfFile(r);
							r = NULL;
						}
					}
					CloseHandle(hMapping);
				}
				CloseHandle(hDataFile);
			}
			else
			{
//				dwFlags &= ~(LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE);
				dwFlags &= ~(LOAD_LIBRARY_AS_DATAFILE | 0x00000040);
				dwFlags |= DONT_RESOLVE_DLL_REFERENCES;
			}
		}
	}
//	if(!(dwFlags & (LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE)))
	if(!(dwFlags & (LOAD_LIBRARY_AS_DATAFILE | 0x00000040)))
	{
		DllFlags = 0;
//		if(dwFlags & (DONT_RESOLVE_DLL_REFERENCES | LOAD_LIBRARY_AS_IMAGE_RESOURCE))
		if(dwFlags & (DONT_RESOLVE_DLL_REFERENCES | 0x00000020))
			DllFlags |= 0x00000002;
//		if(p_LdrLoadDll(NULL, &DllFlags, &us, &r) == STATUS_SUCCESS)
		if(p_LdrLoadDll(NULL, &DllFlags, &us, &r) == 0)
		{
		}
		else
			r = NULL;
	}
	return r;
}

// �t�@�C����MD5�n�b�V�����擾
BOOL GetMD5HashOfFile(LPCWSTR Filename, void* pHash)
{
	BOOL bResult;
	HCRYPTPROV hProv;
	HCRYPTHASH hHash;
	HANDLE hFile;
	DWORD Size;
	void* pData;
	DWORD dw;
	bResult = FALSE;
	if(CryptAcquireContextW(&hProv, NULL, NULL, PROV_RSA_FULL, 0) || CryptAcquireContextW(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_NEWKEYSET))
	{
		if(CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash))
		{
			if((hFile = CreateFileW(Filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) != INVALID_HANDLE_VALUE)
			{
				Size = GetFileSize(hFile, NULL);
				if(pData = VirtualAlloc(NULL, Size, MEM_COMMIT, PAGE_READWRITE))
				{
					VirtualLock(pData, Size);
					if(ReadFile(hFile, pData, Size, &dw, NULL))
					{
						if(CryptHashData(hHash, (BYTE*)pData, Size, 0))
						{
							dw = 16;
							if(CryptGetHashParam(hHash, HP_HASHVAL, (BYTE*)pHash, &dw, 0))
								bResult = TRUE;
						}
					}
					VirtualUnlock(pData, Size);
					VirtualFree(pData, Size, MEM_DECOMMIT);
				}
				CloseHandle(hFile);
			}
			CryptDestroyHash(hHash);
		}
		CryptReleaseContext(hProv, 0);
	}
	return bResult;
}

// DLL�̃n�b�V����o�^
BOOL RegisterTrustedModuleMD5Hash(void* pHash)
{
	BOOL bResult;
	BYTE NullHash[16] = {0};
	int i;
	bResult = FALSE;
	if(FindTrustedModuleMD5Hash(pHash))
		bResult = TRUE;
	else
	{
		i = 0;
		while(i < MAX_TRUSTED_MD5_HASH_TABLE)
		{
			if(memcmp(&g_TrustedMD5HashTable[i], &NullHash, 16) == 0)
			{
				memcpy(&g_TrustedMD5HashTable[i], pHash, 16);
				bResult = TRUE;
				break;
			}
			i++;
		}
	}
	return bResult;
}

// DLL�̃n�b�V���̓o�^������
BOOL UnregisterTrustedModuleMD5Hash(void* pHash)
{
	BOOL bResult;
	BYTE NullHash[16] = {0};
	int i;
	bResult = FALSE;
	i = 0;
	while(i < MAX_TRUSTED_MD5_HASH_TABLE)
	{
		if(memcmp(&g_TrustedMD5HashTable[i], pHash, 16) == 0)
		{
			memcpy(&g_TrustedMD5HashTable[i], &NullHash, 16);
			bResult = TRUE;
			break;
		}
		i++;
	}
	return bResult;
}

// �M���ł��Ȃ�DLL���A�����[�h
BOOL UnloadUntrustedModule()
{
	BOOL bResult;
	wchar_t* pw0;
	HANDLE hSnapshot;
	MODULEENTRY32 me;
	DWORD Length;
	bResult = FALSE;
	pw0 = NULL;
	if((hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId())) != INVALID_HANDLE_VALUE)
	{
		bResult = TRUE;
		me.dwSize = sizeof(MODULEENTRY32);
		if(Module32First(hSnapshot, &me))
		{
			do
			{
				Length = MAX_PATH;
				FreeDuplicatedString(pw0);
				if(pw0 = AllocateStringW(Length))
				{
					if(GetModuleFileNameW(me.hModule, pw0, Length) > 0)
					{
						while(pw0)
						{
							if(GetModuleFileNameW(me.hModule, pw0, Length) + 1 <= Length)
								break;
							Length = Length * 2;
							FreeDuplicatedString(pw0);
							pw0 = AllocateStringW(Length);
						}
					}
				}
				if(pw0)
				{
					if(!IsModuleTrusted(pw0))
					{
						if(me.hModule != GetModuleHandleW(NULL))
						{
							while(FreeLibrary(me.hModule))
							{
							}
							if(GetModuleFileNameW(me.hModule, pw0, Length) > 0)
							{
								bResult = FALSE;
								break;
							}
						}
					}
				}
				else
				{
					bResult = FALSE;
					break;
				}
			}
			while(Module32Next(hSnapshot, &me));
		}
		CloseHandle(hSnapshot);
	}
	FreeDuplicatedString(pw0);
	return bResult;
}

// �֐��|�C���^���g�p�\�ȏ�Ԃɏ�����
BOOL InitializeLoadLibraryHook()
{
	BOOL bResult;
	HMODULE hModule;
	bResult = TRUE;
	if(!(hModule = GetModuleHandleW(L"kernel32.dll")))
		bResult = FALSE;
	if(!(GET_FUNCTION(hModule, LoadLibraryA)))
		bResult = FALSE;
	if(!(GET_FUNCTION(hModule, LoadLibraryW)))
		bResult = FALSE;
	if(!(GET_FUNCTION(hModule, LoadLibraryExA)))
		bResult = FALSE;
	if(!(GET_FUNCTION(hModule, LoadLibraryExW)))
		bResult = FALSE;
	if(!(hModule = GetModuleHandleW(L"ntdll.dll")))
		bResult = FALSE;
	if(!(GET_FUNCTION(hModule, LdrLoadDll)))
		bResult = FALSE;
	if(!(GET_FUNCTION(hModule, LdrGetDllHandle)))
		bResult = FALSE;
	if(!(GET_FUNCTION(hModule, RtlImageNtHeader)))
		bResult = FALSE;
	if(!(hModule = LoadLibraryW(L"wintrust.dll")))
		bResult = FALSE;
	if(!(GET_FUNCTION(hModule, CryptCATAdminCalcHashFromFileHandle)))
		bResult = FALSE;
	return bResult;
}

// SetWindowsHookEx�΍�
// DLL Injection���ꂽ�ꍇ�͏��h_LoadLibrary�n�֐��Ńg���b�v�\
BOOL EnableLoadLibraryHook(BOOL bEnable)
{
	BOOL bResult;
	bResult = FALSE;
	if(bEnable)
	{
		bResult = TRUE;
#ifdef USE_CODE_HOOK
		if(!SET_HOOK_FUNCTION(LoadLibraryA))
			bResult = FALSE;
		if(!SET_HOOK_FUNCTION(LoadLibraryW))
			bResult = FALSE;
		if(!SET_HOOK_FUNCTION(LoadLibraryExA))
			bResult = FALSE;
		if(!SET_HOOK_FUNCTION(LoadLibraryExW))
			bResult = FALSE;
#endif
#ifdef USE_IAT_HOOK
		if(!HookFunctionInIAT(p_LoadLibraryA, h_LoadLibraryA))
			bResult = FALSE;
		if(!HookFunctionInIAT(p_LoadLibraryW, h_LoadLibraryW))
			bResult = FALSE;
		if(!HookFunctionInIAT(p_LoadLibraryExA, h_LoadLibraryExA))
			bResult = FALSE;
		if(!HookFunctionInIAT(p_LoadLibraryExW, h_LoadLibraryExW))
			bResult = FALSE;
#endif
	}
	else
	{
		bResult = TRUE;
#ifdef USE_CODE_HOOK
		if(!END_HOOK_FUNCTION(LoadLibraryA))
			bResult = FALSE;
		if(!END_HOOK_FUNCTION(LoadLibraryW))
			bResult = FALSE;
		if(!END_HOOK_FUNCTION(LoadLibraryExA))
			bResult = FALSE;
		if(!END_HOOK_FUNCTION(LoadLibraryExW))
			bResult = FALSE;
#endif
#ifdef USE_IAT_HOOK
		if(!HookFunctionInIAT(h_LoadLibraryA, p_LoadLibraryA))
			bResult = FALSE;
		if(!HookFunctionInIAT(h_LoadLibraryW, p_LoadLibraryW))
			bResult = FALSE;
		if(!HookFunctionInIAT(h_LoadLibraryExA, p_LoadLibraryExA))
			bResult = FALSE;
		if(!HookFunctionInIAT(h_LoadLibraryExW, p_LoadLibraryExW))
			bResult = FALSE;
#endif
	}
	return bResult;
}

// ReadProcessMemory�AWriteProcessMemory�ACreateRemoteThread�΍�
// TerminateProcess�̂݋���
BOOL RestartProtectedProcess(LPCTSTR Keyword)
{
	BOOL bResult;
	ACL* pACL;
	SID_IDENTIFIER_AUTHORITY sia = SECURITY_WORLD_SID_AUTHORITY;
	PSID pSID;
	SECURITY_DESCRIPTOR sd;
	TCHAR* CommandLine;
	SECURITY_ATTRIBUTES sa;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	bResult = FALSE;
	if(_tcslen(GetCommandLine()) >= _tcslen(Keyword) && _tcscmp(GetCommandLine() + _tcslen(GetCommandLine()) - _tcslen(Keyword), Keyword) == 0)
		return FALSE;
	if(pACL = (ACL*)malloc(sizeof(ACL) + 1024))
	{
		if(InitializeAcl(pACL, sizeof(ACL) + 1024, ACL_REVISION))
		{
			if(AllocateAndInitializeSid(&sia, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &pSID))
			{
				if(AddAccessAllowedAce(pACL, ACL_REVISION, PROCESS_TERMINATE, pSID))
				{
					if(InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION))
					{
						if(SetSecurityDescriptorDacl(&sd, TRUE, pACL, FALSE))
						{
							if(CommandLine = (TCHAR*)malloc(sizeof(TCHAR) * (_tcslen(GetCommandLine()) + _tcslen(Keyword) + 1)))
							{
								_tcscpy(CommandLine, GetCommandLine());
								_tcscat(CommandLine, Keyword);
								sa.nLength = sizeof(SECURITY_ATTRIBUTES);
								sa.lpSecurityDescriptor = &sd;
								sa.bInheritHandle = FALSE;
								GetStartupInfo(&si);
								if(CreateProcess(NULL, CommandLine, &sa, NULL, FALSE, 0, NULL, NULL, &si, &pi))
								{
									CloseHandle(pi.hThread);
									CloseHandle(pi.hProcess);
									bResult = TRUE;
								}
								free(CommandLine);
							}
						}
					}
				}
				FreeSid(pSID);
			}
		}
		free(pACL);
	}
	return bResult;
}

