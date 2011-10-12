// encutf8.cpp : �R���\�[�� �A�v���P�[�V�����̃G���g�� �|�C���g���`���܂��B
//

#include "stdafx.h"


int _tmain(int argc, _TCHAR* argv[])
{
	FILE* fpIn;
	FILE* fpOut;
	fpos_t Length;
	int InLength;
	char* pInBuffer;
	int UTF16Length;
	wchar_t* pUTF16Buffer;
	int OutLength;
	char* pOutBuffer;
	_tsetlocale(LC_ALL, _T(""));
	if(argc != 3)
	{
		_tprintf(_T("�}���`�o�C�g�����i�R�[�h�y�[�W932�܂���Shift JIS�j�ŏ����ꂽ�e�L�X�g�t�@�C����UTF-8�ɃG���R�[�h���܂��B\n"));
		_tprintf(_T("�R�}���h���C��\n"));
		_tprintf(_T("encutf8 [in] [out]\n"));
		_tprintf(_T("[in]    ���̃\�[�X�t�@�C���̃t�@�C����\n"));
		_tprintf(_T("[out]   �ۑ���̃t�@�C����\n"));
		return 0;
	}
	fpIn = _tfopen(argv[1], _T("rb"));
	if(!fpIn)
	{
		_tprintf(_T("�t�@�C��\"%s\"���J���܂���B\n"), argv[1]);
		return 0;
	}
	fseek(fpIn, 0, SEEK_END);
	fgetpos(fpIn, &Length);
	fseek(fpIn, 0, SEEK_SET);
	InLength = Length / sizeof(char);
	pInBuffer = new char[InLength];
	UTF16Length = InLength;
	pUTF16Buffer = new wchar_t[InLength];
	OutLength = InLength * 4;
	pOutBuffer = new char[OutLength];
	if(!pInBuffer || !pUTF16Buffer || !pOutBuffer)
	{
		_tprintf(_T("���������m�ۂł��܂���B\n"));
		return 0;
	}
	fread(pInBuffer, 1, InLength, fpIn);
	fclose(fpIn);
	fpOut = _tfopen(argv[2], _T("wb"));
	if(!fpIn)
	{
		_tprintf(_T("�t�@�C��\"%s\"���쐬�ł��܂���B\n"), argv[2]);
		return 0;
	}
	fwrite("\xEF\xBB\xBF", 1, 3, fpOut);
	UTF16Length = MultiByteToWideChar(CP_ACP, 0, pInBuffer, InLength / sizeof(char), pUTF16Buffer, UTF16Length);
	OutLength = WideCharToMultiByte(CP_UTF8, 0, pUTF16Buffer, UTF16Length, pOutBuffer, OutLength / sizeof(char), NULL, NULL);
	fwrite(pOutBuffer, sizeof(char), OutLength, fpOut);
	fclose(fpOut);
	return 0;
}

