// Copyright(C) 2020 Kurata Sayuri. All rights reserved.
#pragma once
#include <string_view>
#include <tuple>
#include <assert.h>
#include <stdio.h>

inline namespace {
	using namespace std::literals;
	struct filelistparser {
		constexpr static std::tuple mlsd = {
			// �ȉ��̌`���ɑΉ�
			// fact1=value1;fact2=value2;fact3=value3; filename\r\n
			// �s���S�Ȏ����̃z�X�g�����݂��邽�߈ȉ��̌`�������e
			// fact1=value1;fact2=value2;fact3=value3 filename\r\n
			// fact1=value1;fact2=value2;fact3=value3;filename\r\n
			// 1. facts�A2. �t�@�C����
			R"(((?:(?:^|;)[^ ;=]+=[^ ;]+)+)(?:;? |;(?=[^ ]+$))(.+)$)"sv, false
		};
		static inline const auto unix = [] {
			static char pattern[512];
			auto len = _sprintf_p(
				pattern, std::size(pattern),
				// 1. �t�@�C����ʁA2. �p�[�~�b�V�����A(�����N�j�A3. Owner�A�X�L�b�v�A4. �T�C�Y�A(5. �N�A6. ���A7. ���A(8. ���A9. ��)? | 10. ���A11. ���A(12. �N | 13. ���A14. ��))�A15. �t�@�C����
				R"(^ *([-+dfl])([^ ]{9})(?:[0-9]+| +[0-9]+|) +([^ ]+) +.*?([0-9]+) +(?:%1$s[^ 0-9]* *%2$s[^ 0-9]* *%3$s[^ 0-9]*(?: +%4$s[^ 0-9]+%5$s[^ 0-9]*)?|%2$s[^ 0-9]* *%3$s[^ 0-9]* +(?:%1$s[^ 0-9]*|%4$s[^ 0-9]*%5$s[^ 0-9]*)) +(.*)$)",
				"(1[6-9][0-9]{2}|[2-9][0-9]{3})",									// %1$s. Year
				"(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec|0?[1-9]|1[0-2])",	// %2$s. Month
				"(0?[1-9]|[12][0-9]|3[01])",										// %3$s. Day
				"([01]?[0-9]|2[0-3])",												// %4$s. Hour
				"([0-5]?[0-9])"														// %5$s. Minute
			);
			assert(0 < len);
			return std::tuple{ std::string_view{ pattern, (size_t)len }, true };
		}();
		constexpr static std::tuple melcom80 = {
			// 1. �t�@�C����ʁA2. �p�[�~�b�V�����A�����N���A3. Owner�A4. �T�C�Y�A5. ���A6. ���A7. �N�A8. �t�@�C�����A9. �t�@�C�����
			R"(^ *([-+dfl]) +([^ ]{9}) +[0-9]+ +([^ ]+) +([0-9]+) +(JAN|FEB|MAR|APR|MAY|JUN|JUL|AUG|SEP|OCT|NOV|DEC) +([1-9]|[12][0-9]|3[01]) + (1[6-9][0-9]{2}|[2-9][0-9]{3}) +(?=([^ ]{1,14})).{14}(.).*$)"sv, false
		};
		constexpr static std::tuple agilent = {
			// 1. �t�@�C����ʁA2. �p�[�~�b�V�����A�s���A3. Owner�A�s���A4. �T�C�Y�A5. �t�@�C����
			R"(^ *([-+dfl])([^ ]{9}) +[0-9]+ +([0-9]+) +[0-9]+ +([0-9]+) +(.+)$)"sv, false
		};
		static inline const auto dos = [] {
			static char pattern[512];
			auto len = _sprintf_p(
				pattern, std::size(pattern),
				// (1. �N�A2. ��؂�A3. ���A4. �� | 5. ���A6. ��؂�A7. ���A8. �N) �A9. ���A10. ���A(11. �b)?�A12. AMPM�A13. �T�C�Y�A14. �t�@�C����
				R"(^ *(?:%1$s(-|/)%2$s\2%3$s|%2$s(-|/)%3$s\6%1$s) +%4$s:%5$s(?::%6$s)?([AaPp])?[^ ]* +(<DIR>|[0-9]+) +(.*)$)",
				"((?:|1[6-9]?|[2-9][0-9])[0-9]{2})",	// %1$s. Year
				"(0?[1-9]|1[0-2])",						// %2$s. Month
				"(0?[1-9]|[12][0-9]|3[01])",			// %3$s. Day
				"([01]?[0-9]|2[0-3])",					// %4$s. Hour
				"([0-5]?[0-9])",						// %5$s. Minute
				"([0-5]?[0-9])"							// %6$s. Second
			);
			assert(0 < len);
			return std::tuple{ std::string_view{ pattern, (size_t)len }, false };
		}();
		constexpr static std::tuple chameleon = {
			// 1. �t�@�C�����A2. �T�C�Y�A3. ���A4. ���A5. �N�A6. ���A7. ���A�t�@�C�����
			R"(^ *([^ ]+) +(<DIR>|[0-9]+) +(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec|0?[1-9]|1[0-2])(?: +|-)(0?[1-9]|[12][0-9]|3[01])(?: +|-)((?:|1[6-9]?|[2-9][0-9])[0-9]{2}) +([01]?[0-9]|2[0-3]):([0-5]?[0-9]) +[^ ]*$)"sv, false
		};
		constexpr static std::tuple os2 = {
			// 1. �T�C�Y�A(����)?�A(2. �t�@�C�����)?�A3. ���A4. ���A5. �N�A6. ���A7. ���A8. �t�@�C����
			R"(^ *([0-9]+) +(?:[^ ]+ +)?(DIR +)?(0[1-9]|1[0-2])-(0[1-9]|[12][0-9]|3[01])-((?:|1[6-9]?|[2-9][0-9])[0-9]{2}) +([01][0-9]|2[0-3]):([0-5][0-9]) +([^ ]+)$)"sv, false
		};
		constexpr static std::tuple allied = {
			// 1. �T�C�Y�A2. �t�@�C�����A�j���A3. ���A4. ���A5. ���A6. ���A7. �b�A8. �N
			R"(^ *(<dir>|[0-9]+) +([^ ]+) +(?:Sun|Mon|Tue|Wed|Thu|Fri|Sat) +(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec) +(0?[1-9]|[12][0-9]|3[01]) +([01][0-9]|2[0-3]):([0-5][0-9]):([0-5][0-9]) +(1[6-9][0-9]{2}|[2-9][0-9]{3})$)"sv, false
		};
		constexpr static std::tuple shibasoku = {
			// 1. �T�C�Y�A2. ���A3. ���A4. �N�A5. ���A6. ���A7. �b�A8. �t�@�C�����A9. �t�@�C�����
			R"(^ *([0-9]+) +(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)-(0[1-9]|[12][0-9]|3[01])-(1[6-9][0-9]{2}|[2-9][0-9]{3}) +([01][0-9]|2[0-3]):([0-5][0-9]):([0-5][0-9]) +([^ ]+) +(<DIR>)?$)"sv, false
		};
		constexpr static std::tuple as400 = {
			// 1. Owner�A2. �T�C�Y�A3. �N�A4. ���A5. ���A6. ���A7. ���A8. �b�A�s���A9. �t�@�C�����A10. �t�@�C�����
			R"(^ *([^ ]+) +([0-9]+) +((?:|1[6-9]?|[2-9][0-9])[0-9]{2})/(0[1-9]|1[0-2])/(0[1-9]|[12][0-9]|3[01]) +([01][0-9]|2[0-3]):([0-5][0-9]):([0-5][0-9]) +[^ ]+ +([^/ ]+)(/)?$)"sv, false
		};
		constexpr static std::tuple m1800 = {
			// �t�@�C����ʁA1. �p�[�~�b�V�����A�s���A�s���A�s���A�s���A(2. �N�A3. ���A4. �� | **.**.**)�A5. �t�@�C�����A6. �t�@�C�����
			R"(^ *[^ ]([^ ]{3}) +[^ ]+ +[0-9*]+ +[0-9*]+ +[^ ]+ +(?:((?:|1[6-9]?|[2-9][0-9])[0-9]{2})\.(0[1-9]|1[0-2])\.(0[1-9]|[12][0-9]|3[01])|\*{2,4}\.\*\*\.\*\*) +([^/ ]+)(/)? *$)"sv, false
		};
		constexpr static std::tuple gp6000 = {
			// 1. �t�@�C����ʁA2. �p�[�~�b�V�����A3. �N�A4. ���A5. ���A6. ���A7. ���A8. �b�A9. Owner�A�s���A10. �T�C�Y�A11. �t�@�C����
			R"(^ *([^ ])([^ ]{9}) +((?:|1[6-9]?|[2-9][0-9])[0-9]{2})\.(0[1-9]|1[0-2])\.(0[1-9]|[12][0-9]|3[01]) +([01][0-9]|2[0-3]):([0-5][0-9]):([0-5][0-9]) +([^ ]+) +[^ ]+ +([0-9]+) +(.+)$)"sv, false
		};
		constexpr static std::tuple os7 = {
			// 1. �t�@�C����ʁA2. �p�[�~�b�V�����A(�s���A3. �T�C�Y)?�A4. �N�A5. ���A6. ���A7. ���A8. ���A9. �b�A10. �t�@�C����
			R"(^ *([^ ])([^ ]{9}) +(?:[^ ]+ +([0-9]+) +)?((?:|1[6-9]?|[2-9][0-9])[0-9]{2})/(0[1-9]|1[0-2])/(0[1-9]|[12][0-9]|3[01]) +([01][0-9]|2[0-3]):([0-5][0-9]):([0-5][0-9]) +(.+?) *$)"sv, false
		};
		constexpr static std::tuple os9 = {
			// 1. Owner�A2. �N�A3. ���A4. ���A5. ���A6. ���A7. �t�@�C����ʁA�s���A8. �T�C�Y�A9. �t�@�C����
			R"(^ *([^ ]+) +((?:|1[6-9]?|[2-9][0-9])[0-9]{2})/(0[1-9]|1[0-2])/(0[1-9]|[12][0-9]|3[01]) +([01][0-9]|2[0-3])([0-5][0-9]) +([^ ])[^ ]+ +[^ ]+ +([0-9]+) +(.+)$)"sv, false
		};
		constexpr static std::tuple ibm = {
			// �s���A�s���A1. �N�A2. ���A3. ���A�s���A�s���A�s���A�s���A�s���A4. �t�@�C����ʁA5. �t�@�C����
			R"(^ *[^ ]+ +[0-9]+ +(1[6-9][0-9]{2}|[2-9][0-9]{3})/(0[1-9]|1[0-2])/(0[1-9]|[12][0-9]|3[01]) +[0-9]+ +[0-9]+ +[^ ]+ +[0-9]+ +[0-9]+ +P(O|S) +(.+)$)"sv, false
		};
		constexpr static std::tuple stratus = {
			// (1. �t�@�C�����X�g | 2. �f�B���N�g�����X�g | 3. �����N���X�g | �s���A4. �T�C�Y�A(5. Owner)?�A6. �N�A7. ���A8. ���A9. ���A10. ���A11. �b�A12. �t�@�C����)
			R"(^ *(?:(Files:.*)|(Dirs:.*)|(Links:.*)|[^ ]+ +([0-9]+) +(?:([^ ]+) +)?((?:|1[6-9]?|[2-9][0-9])[0-9]{2})-(0[1-9]|1[0-2])-(0[1-9]|[12][0-9]|3[01]) +([01][0-9]|2[0-3]):([0-5][0-9]):([0-5][0-9]) +(.+))$)"sv, false
		};
		constexpr static std::tuple vms = {
			// 1. OpenVMS�p�t�@�C�����A2. �t�@�C�����A3. �t�@�C����ʁA4. �T�C�Y�A5. ���A6. ���A7. �N�A8. ���A9. ���A10. �b�A�s��
			R"(^ *(([^ ]+?)(\.DIR;[^ ]*)?) +([0-9]+)/[0-9]+ +(0?[1-9]|[12][0-9]|3[01])-(JAN|FEB|MAR|APR|MAY|JUN|JUL|AUG|SEP|OCT|NOV|DEC)-(1[6-9][0-9]{2}|[2-9][0-9]{3}) +([01][0-9]|2[0-3]):([0-5][0-9]):([0-5][0-9]) +.+$)"sv, false
		};
		constexpr static std::tuple irmx = {
			// 1. �t�@�C�����A2. �t�@�C����ʁA�s��?�A�s���A3. �T�C�Y�A�s���A�s���A4. Owner�A5. ���A6. ���A7. �N
			R"(^ *([^ ]+)( +DR)?.+? +[0-9]+ +([0-9,]+) +[0-9,]+ +[0-9]+ +(.+?) +(0[1-9]|[12][0-9]|3[01]) +(JAN|FEB|MAR|APR|MAY|JUN|JUL|AUG|SEP|OCT|NOV|DEC) +((?:|1[6-9]?|[2-9][0-9])[0-9]{2})$)"sv, false
		};
		constexpr static std::tuple tandem = {
			// 1. �t�@�C�����AOpen���A2. Code�A3. �T�C�Y�A4. ���A5. ���A6. �N�A7. ���A8. ���A9. �b�A10. Owner�A�s��
			R"(^ *([^ ]+) +(?:O +)?([0-9]+) +([0-9]+) +(0?[1-9]|[12][0-9]|3[01])-(Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)-((?:|1[6-9]?|[2-9][0-9])[0-9]{2}) +([01][0-9]|2[0-3]):([0-5][0-9]):([0-5][0-9]) +(.+?) +[^ ]+$)"sv, false
		};
	};
}