

#include "SplitUdr.h"
#include <string>
#include <string_view>
#include <vector>


namespace {

constexpr size_t MAX_SEGMENT_SIZE = 65535;
constexpr size_t MAX_VARCHAR_SIZE = 32765;

}

using namespace Firebird;


/***
create procedure split_str (
	in_txt varchar(8191) character set utf8,
	in_separator varchar(10) character set utf8
) returns (
	out_txt varchar(8191) character set utf8
)
	external name 'splitudr!split_s'
	engine udr;
***/
FB_UDR_BEGIN_PROCEDURE(split_s)
FB_UDR_MESSAGE(InMessage,
	(FB_VARCHAR(32765), txt)
	(FB_VARCHAR(255), separator)
);

FB_UDR_MESSAGE(OutMessage,
	(FB_VARCHAR(32765), txt)
);

FB_UDR_EXECUTE_PROCEDURE
{
	if (in->txtNull || in->separatorNull)
	{
		stopFlag = true;
		out->txtNull = FB_TRUE;
		out->txt.length = 0;
	}
	else
	{
		str = std::string_view(in->txt.str, in->txt.length);

		delim = std::string_view(in->separator.str, in->separator.length);
		delta = delim.length();
	}
}


std::string_view str;
std::string_view delim;
size_t prev = 0;
size_t next = 0;
size_t delta = 0;
bool stopFlag = false;

FB_UDR_FETCH_PROCEDURE
{
	if (stopFlag)
	{
		return false;
	}
	out->txtNull = FB_FALSE;
	if ((next = str.find(delim, prev)) != std::string::npos) {
		// ���� ������� � ������ �����������
		// ���������� ������ ����� �������������		
		out->txt.length = static_cast<ISC_SHORT>(next - prev);
		str.copy(out->txt.str, out->txt.length, prev);
		prev = next + delta;
		stopFlag = prev >= str.length();
		return true;
	}
	next = str.length();
	out->txt.length = static_cast<ISC_SHORT>(next - prev);
	str.copy(out->txt.str, out->txt.length, prev);
	prev = next + delta;
	stopFlag = prev >= str.length();
	return true;
}
FB_UDR_END_PROCEDURE

/***
create procedure split (
	in_txt blob sub_type text character set utf8,
	in_separator varchar(10) character set utf8
) returns (
	out_txt varchar(8191) character set utf8
)
	external name 'splitudr!split'
	engine udr;
***/
FB_UDR_BEGIN_PROCEDURE(split)
FB_UDR_MESSAGE(InMessage,
    (FB_BLOB, txt)
    (FB_VARCHAR(255), separator)
);

FB_UDR_MESSAGE(OutMessage,
    (FB_VARCHAR(32765), txt)
);

FB_UDR_EXECUTE_PROCEDURE
{
	if (in->txtNull || in->separatorNull)
	{
		stopFlag = true;
		out->txtNull = FB_TRUE;
		out->txt.length = 0;
	}
	else
	{
		delim = std::string_view(in->separator.str, in->separator.length);
		delta = delim.length();

		att.reset(context->getAttachment(status));
		tra.reset(context->getTransaction(status));

		blob.reset(att->openBlob(status, tra, &in->txt, 0, nullptr));
		// ������ ������ ~32K����
		std::vector<char> vBuffer(MAX_VARCHAR_SIZE + 1);
		{
			char* buffer = vBuffer.data();
			for (size_t n = 0; !eof && n < MAX_VARCHAR_SIZE; ) {
				unsigned int l = 0;
				switch (blob->getSegment(status, MAX_VARCHAR_SIZE, buffer, &l))
				{
				case IStatus::RESULT_OK:
				case IStatus::RESULT_SEGMENT:
                    str.append(buffer, l);
					n += l;
					continue;
				default:
					blob->close(status);
					blob.release();
					eof = true;
					break;
				}
			}
		}
	}
}

AutoRelease<IAttachment> att;
AutoRelease<ITransaction> tra;
AutoRelease<IBlob> blob;

std::string str;
std::string_view delim;
size_t prev = 0;
size_t next = 0;
size_t delta = 0;
bool stopFlag = false;
bool eof = false;

FB_UDR_FETCH_PROCEDURE
{
	if (stopFlag)
	{
		return false;
	}
	out->txtNull = FB_FALSE;
	if ((next = str.find(delim, prev)) != std::string::npos) {
		// ���� ������� � ������ �����������
		// ���������� ������ ����� �������������		
		if (next - prev > 32765) {
			ISC_STATUS statusVector[] = {
				 isc_arg_gds, isc_random,
				 isc_arg_string, (ISC_STATUS)"Output buffer overflow",
				 isc_arg_end
			};
			throw Firebird::FbException(status, statusVector);
		}
		out->txt.length = static_cast<ISC_SHORT>(next - prev);
		str.copy(out->txt.str, out->txt.length, prev);
		prev = next + delta;
		return true;
	}
	// ������ ����� ���������� ����������� �� ����������� ������,
	// ����������� ����� ���� � �� ����������� ����� BLOB
	if (!eof) {
		// ���� BLOB �������� �� ���������, ��
		// ������ ��������� ~32K����
		std::string s;
		std::vector<char> vBuffer(MAX_VARCHAR_SIZE + 1);
		{
			char* buffer = vBuffer.data();
			for (size_t n = 0; !eof && n < MAX_VARCHAR_SIZE; ) {
				unsigned int l = 0;
				switch (blob->getSegment(status, MAX_VARCHAR_SIZE, buffer, &l))
				{
				case IStatus::RESULT_OK:
				case IStatus::RESULT_SEGMENT:
                    s.append(buffer, l);
					n += l;
					continue;
				default:
					blob->close(status);
					blob.release();
					eof = true;
					break;
				}
			}
		}
		// ������� �� ������ �� ����� �����
        // ����� ���������� �����������
		str.erase(0, prev);
		prev = 0;
		// � ��������� � �� ����������� �� BLOB
		str += s;
		
		// ���� �����������
		next = str.find(delim, prev);
		if (next == std::string::npos)
			next = str.length();
	}
	else {
		next = str.length();
	}
	if (next - prev > 32765) {
		ISC_STATUS statusVector[] = {
			 isc_arg_gds, isc_random,
			 isc_arg_string, (ISC_STATUS)"Output buffer overflow",
			 isc_arg_end
		};
		throw Firebird::FbException(status, statusVector);
	}
	out->txt.length = static_cast<ISC_SHORT>(next - prev);
	str.copy(out->txt.str, out->txt.length, prev);
	prev = next + delta;
	stopFlag = eof && prev >= str.length();
	return true;
}
FB_UDR_END_PROCEDURE

/***
create procedure split_words_s (
	in_txt varchar(8191) character set utf8,
	in_separators varchar(50) character set utf8 default null
) returns (
	out_txt varchar(8191) character set utf8
)
	external name 'splitudr!strtok_s'
	engine udr;
***/
FB_UDR_BEGIN_PROCEDURE(strtok_s)
FB_UDR_MESSAGE(InMessage,
	(FB_VARCHAR(32765), txt)
	(FB_VARCHAR(255), separators)
);

FB_UDR_MESSAGE(OutMessage,
	(FB_VARCHAR(32765), txt)
);

FB_UDR_EXECUTE_PROCEDURE
{
	if (in->txtNull)
	{
		stopFlag = true;
		out->txtNull = FB_TRUE;
		out->txt.length = 0;
	}
	else
	{
		str = std::string_view(in->txt.str, in->txt.length);

		if (in->separatorsNull) {
			separators = std::string_view(" \n\r\t,.?!:;/\\|<>[]{}()@#$%^&*-+='\"~`");
		}
		else
		   separators = std::string_view(in->separators.str, in->separators.length);

	}
}

std::string_view str;
std::string_view separators;
size_t prev = 0;
size_t next = 0;
bool stopFlag = false;

FB_UDR_FETCH_PROCEDURE
{
	if (stopFlag)
	{
		return false;
	}
	out->txtNull = FB_FALSE;
	// ���� ������ �� �������� separators � ������ str ������� � ������� prev
	while ((next = str.find_first_of(separators, prev)) != std::string::npos) {
		// ���� ������� � ������ �����������
		// ���������� ������ ����� �������������	
		const size_t length = next - prev;
		// ���� ������ ���������� ������ ���� ��������� �����������
		if (length == 0) {
			prev = next + 1;
			continue;
		}
		// �������� ��������� � �������� ���������
		out->txt.length = static_cast<ISC_SHORT>(length);
		str.copy(out->txt.str, out->txt.length, prev);

		prev = next + 1;
		return true;
	}
	// �� ������ ����������� �� �������, 
	// ������ �������� ������ �� ����������� ����������� � �� ����� ������ str
	next = str.length();

	const size_t length = next - prev;
	// ���� ������ ���������� ������ ������ ��������� �� ����������
	if (length == 0) {
		return false;
	}
	// �������� ��������� � �������� ���������
	out->txt.length = static_cast<ISC_SHORT>(length);
	str.copy(out->txt.str, out->txt.length, prev);
	prev = next + 1;
	// ������������� ����� ���������
	stopFlag = prev >= str.length();
	return true;

}
FB_UDR_END_PROCEDURE

/***
create procedure split_words (
	in_txt blob sub_type text character set utf8,
	in_separators varchar(50) character set utf8 default null
) returns (
	out_txt varchar(8191) character set utf8
)
	external name 'splitudr!strtok'
	engine udr;
***/
FB_UDR_BEGIN_PROCEDURE(strtok)
FB_UDR_MESSAGE(InMessage,
	(FB_BLOB, txt)
	(FB_VARCHAR(255), separators)
);

FB_UDR_MESSAGE(OutMessage,
	(FB_VARCHAR(32765), txt)
);

FB_UDR_EXECUTE_PROCEDURE
{
	if (in->txtNull)
	{
		stopFlag = true;
		out->txtNull = FB_TRUE;
		out->txt.length = 0;
	}
	else
	{
		if (in->separatorsNull) {
			separators = std::string_view(" \n\r\t,.?!:;/\\|<>[]{}()@#$%^&*-+='\"~`");
		}
		else
		   separators = std::string_view(in->separators.str, in->separators.length);

		att.reset(context->getAttachment(status));
		tra.reset(context->getTransaction(status));

		blob.reset(att->openBlob(status, tra, &in->txt, 0, nullptr));
		// ������ ������ ~32K����
		std::vector<char> vBuffer(MAX_VARCHAR_SIZE + 1);
		{
			char* buffer = vBuffer.data();
			for (size_t n = 0; !eof && n < MAX_VARCHAR_SIZE; ) {
				unsigned int l = 0;
				switch (blob->getSegment(status, MAX_VARCHAR_SIZE, buffer, &l))
				{
				case IStatus::RESULT_OK:
				case IStatus::RESULT_SEGMENT:
					str.append(buffer, l);
					n += l;
					continue;
				default:
					blob->close(status);
					blob.release();
					eof = true;
					break;
				}
			}
		}
	}
}


AutoRelease<IAttachment> att;
AutoRelease<ITransaction> tra;
AutoRelease<IBlob> blob;

std::string str;
std::string_view separators;
size_t prev = 0;
size_t next = 0;
bool stopFlag = false;
bool eof = false;

FB_UDR_FETCH_PROCEDURE
{
	if (stopFlag)
	{
		return false;
	}
	out->txtNull = FB_FALSE;
	// ���� ������ �� �������� separators � ������ str ������� � ������� prev
	while ((next = str.find_first_of(separators, prev)) != std::string::npos) {
		// ���� ������� � ������ �����������
		// ���������� ������ ����� �������������	
		const size_t length = next - prev;
		// ���� ������ ���������� ������ ���� ��������� �����������
		if (length == 0) {
			prev = next + 1;
			continue;
		}
		if (length > 32765) {
			ISC_STATUS statusVector[] = {
				 isc_arg_gds, isc_random,
				 isc_arg_string, (ISC_STATUS)"Output buffer overflow",
				 isc_arg_end
			};
			throw Firebird::FbException(status, statusVector);
		}
		// �������� ��������� � �������� ���������
		out->txt.length = static_cast<ISC_SHORT>(length);
		str.copy(out->txt.str, out->txt.length, prev);
		
		prev = next + 1;
		return true;
	}
	// ������ ����� ���������� ����������� �� ����������� ������,
	// ����������� ����� ���� � �� ����������� ����� BLOB
	if (!eof) {
		// ���� BLOB �������� �� ���������, ��
		// ������ ��������� ~32K����
		std::string s;
		std::vector<char> vBuffer(MAX_VARCHAR_SIZE + 1);
		{
			char* buffer = vBuffer.data();
			for (size_t n = 0; !eof && n < MAX_VARCHAR_SIZE; ) {
				unsigned int l = 0;
				switch (blob->getSegment(status, MAX_VARCHAR_SIZE, buffer, &l))
				{
				case IStatus::RESULT_OK:
				case IStatus::RESULT_SEGMENT:
					s.append(buffer, l);
					n += l;
					continue;
				default:
					blob->close(status);
					blob.release();
					eof = true;
					break;
				}
			}
		}
		// ������� �� ������ �� ����� �����
		// ����� ���������� �����������
		str.erase(0, prev);
		prev = 0;
		// � ��������� � �� ����������� �� BLOB
		str.append(s);

		// ���� ������ �� �������� separators � ������ str ������� � ������� prev
		next = str.find_first_of(separators, prev);
		if (next == std::string::npos)
			next = str.length();
	}
	else {
		next = str.length();
	}
	while (true) {
		const size_t length = next - prev;
		// ���� ������ ���������� ������ ���� ��������� �����������
		if (length == 0) {
			prev = next + 1;
			next = str.find_first_of(separators, prev);
			if (next != std::string::npos)
				continue;
			else
				return false;
		}
		if (length > 32765) {
			ISC_STATUS statusVector[] = {
				 isc_arg_gds, isc_random,
				 isc_arg_string, (ISC_STATUS)"Output buffer overflow",
				 isc_arg_end
			};
			throw Firebird::FbException(status, statusVector);
		}
		// �������� ��������� � �������� ���������
		out->txt.length = static_cast<ISC_SHORT>(length);
		str.copy(out->txt.str, out->txt.length, prev);
		prev = next + 1;
		// ������������� ����� ���������
		stopFlag = eof && prev >= str.length();
		return true;
	}
}
FB_UDR_END_PROCEDURE

FB_UDR_IMPLEMENT_ENTRY_POINT