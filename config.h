
/* OPENVMS�p�̃R�[�h��L���ɂ���B����a�쐬�̃p�b�`��g�ݍ��݂܂����B */
#define HAVE_OPENVMS

//�S�̂ɉe������ݒ�͂����ɋL�q����\��
//������UTF-8�Ƃ��Ĉ����}���`�o�C�g�������C�h����API���b�p�[���g�p����
#include "mbswrapper.h"
//�g�p����CPU��1�Ɍ��肷��i�}���`�R�ACPU�̓�������Ńt�@�C���ʐM���ɃN���b�V������o�O�΍�j
#define DISABLE_MULTI_CPUS
//�l�b�g���[�N�o�b�t�@�𖳌��ɂ���i�ʐM���~��Ƀ����[�g�̃f�B���N�g�����\������Ȃ��o�O�΍�j
//#define DISABLE_NETWORK_BUFFERS

