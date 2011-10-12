// protectprocess.h
// Copyright (C) 2011 Suguru Kawamoto
// �v���Z�X�̕ی�

#ifndef __PROTECTPROCESS_H__
#define __PROTECTPROCESS_H__

#define ENABLE_PROCESS_PROTECTION

// ���̒�����1�̂ݗL���ɂ���
// �t�b�N��̊֐��̃R�[�h������������
// �S�Ă̌Ăяo�����t�b�N�\���������I�ɓ�d�Ăяo���ɑΉ��ł��Ȃ�
#define USE_CODE_HOOK
// �t�b�N��̊֐��̃C���|�[�g�A�h���X�e�[�u��������������
// ��d�Ăяo�����\�����Ăяo�����@�ɂ���Ă̓t�b�N����������
//#define USE_IAT_HOOK

typedef HMODULE (WINAPI* _LoadLibraryA)(LPCSTR);
typedef HMODULE (WINAPI* _LoadLibraryW)(LPCWSTR);
typedef HMODULE (WINAPI* _LoadLibraryExA)(LPCSTR, HANDLE, DWORD);
typedef HMODULE (WINAPI* _LoadLibraryExW)(LPCWSTR, HANDLE, DWORD);

#ifndef DO_NOT_REPLACE

#ifdef USE_IAT_HOOK

// �ϐ��̐錾
#define EXTERN_HOOK_FUNCTION_VAR(name) extern _##name p_##name;

#undef LoadLibraryA
#define LoadLibraryA p_LoadLibraryA
EXTERN_HOOK_FUNCTION_VAR(LoadLibraryA)
#undef LoadLibraryW
#define LoadLibraryW p_LoadLibraryW
EXTERN_HOOK_FUNCTION_VAR(LoadLibraryW)
#undef LoadLibraryExA
#define LoadLibraryExA p_LoadLibraryExA
EXTERN_HOOK_FUNCTION_VAR(LoadLibraryExA)
#undef LoadLibraryExW
#define LoadLibraryExW p_LoadLibraryExW
EXTERN_HOOK_FUNCTION_VAR(LoadLibraryExW)

#endif

#endif

// ���[�h�ς݂̃��W���[���͌������p�X
#define PROCESS_PROTECTION_LOADED 0x00000001
// ���W���[���ɖ��ߍ��܂ꂽAuthenticode����������
#define PROCESS_PROTECTION_BUILTIN 0x00000002
// �T�C�h�o�C�T�C�h��Authenticode����������
#define PROCESS_PROTECTION_SIDE_BY_SIDE 0x00000004
// WFP�ɂ��ی쉺�ɂ��邩������
#define PROCESS_PROTECTION_SYSTEM_FILE 0x00000008
// Authenticode�����̗L�������𖳎�
#define PROCESS_PROTECTION_EXPIRED 0x00000010
// Authenticode�����̔��s���𖳎�
#define PROCESS_PROTECTION_UNAUTHORIZED 0x00000020

#define PROCESS_PROTECTION_NONE 0
#define PROCESS_PROTECTION_DEFAULT PROCESS_PROTECTION_HIGH
#define PROCESS_PROTECTION_HIGH (PROCESS_PROTECTION_BUILTIN | PROCESS_PROTECTION_SIDE_BY_SIDE | PROCESS_PROTECTION_SYSTEM_FILE)
#define PROCESS_PROTECTION_MEDIUM (PROCESS_PROTECTION_HIGH | PROCESS_PROTECTION_LOADED | PROCESS_PROTECTION_EXPIRED)
#define PROCESS_PROTECTION_LOW (PROCESS_PROTECTION_MEDIUM | PROCESS_PROTECTION_UNAUTHORIZED)

HMODULE System_LoadLibrary(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);
void SetProcessProtectionLevel(DWORD Level);
BOOL GetSHA1HashOfFile(LPCWSTR Filename, void* pHash);
BOOL RegisterTrustedModuleSHA1Hash(void* pHash);
BOOL UnregisterTrustedModuleSHA1Hash(void* pHash);
BOOL UnloadUntrustedModule();
BOOL InitializeLoadLibraryHook();
BOOL EnableLoadLibraryHook(BOOL bEnable);
BOOL RestartProtectedProcess(LPCTSTR Keyword);

#endif

