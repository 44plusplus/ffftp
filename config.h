
/* OPENVMS�p�̃R�[�h��L���ɂ���B����a�쐬�̃p�b�`��g�ݍ��݂܂����B */
#define HAVE_OPENVMS

// �S�̂ɉe������ݒ�͂����ɋL�q����\��
// ������UTF-8�Ƃ��Ĉ����}���`�o�C�g�������C�h����API���b�p�[���g�p����
#include "mbswrapper.h"
// OpenSSL�p�\�P�b�g���b�p�[���g�p����
#include "socketwrapper.h"
// �g�p����CPU��1�Ɍ��肷��i�}���`�R�ACPU�̓�������Ńt�@�C���ʐM���ɃN���b�V������o�O�΍�j
#define DISABLE_MULTI_CPUS
// �t�@�C���]���p�̃l�b�g���[�N�o�b�t�@�𖳌��ɂ���i�ʐM���~��Ƀ����[�g�̃f�B���N�g�����\������Ȃ��o�O�΍�j
//#define DISABLE_TRANSFER_NETWORK_BUFFERS
// �R���g���[���p�̃l�b�g���[�N�o�b�t�@�𖳌��ɂ���i�t���[�Y�΍�j
#define DISABLE_CONTROL_NETWORK_BUFFERS
// JRE32.DLL�𖳌��ɂ���iUTF-8�ɔ�Ή��̂��߁j
#define DISABLE_JRE32DLL

