

#include "SplitUdr.h"
#include <string>
#include <string_view>
#include <vector>


namespace {

constexpr size_t BUFFER_SIZE = 32768;
constexpr size_t MAX_VARCHAR_SIZE = 32765;

unsigned int readBlobData(Firebird::ThrowStatusWrapper* status, Firebird::IBlob* blob, unsigned int buffer_size, char* buffer)
{
	unsigned int readBytes = 0;
	while (buffer_size > 0) {
		unsigned int buffer_length = std::min(buffer_size, 32768u);
		unsigned int segment_length = 0;
		auto result = blob->getSegment(status, buffer_length, buffer + static_cast<size_t>(readBytes), &segment_length);
		switch (result)
		{
		case Firebird::IStatus::RESULT_OK:
		case Firebird::IStatus::RESULT_SEGMENT:
			buffer_size -= segment_length;
			readBytes += segment_length;
			break;
		default:
			buffer_size = 0;
			break;
		}
	}
	return readBytes;
}

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
	}
}


std::string_view str;
std::string_view delim;
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
	if ((next = str.find(delim, prev)) != std::string::npos) {
		// пока находим в строке разделитель
		// возвращаем строки между разделителями		
		out->txt.length = static_cast<ISC_SHORT>(next - prev);
		str.copy(out->txt.str, out->txt.length, prev);
		prev = next + static_cast<size_t>(in->separator.length);
		stopFlag = prev >= str.length();
		return true;
	}
	next = str.length();
	out->txt.length = static_cast<ISC_SHORT>(next - prev);
	str.copy(out->txt.str, out->txt.length, prev);
	prev = next + static_cast<size_t>(in->separator.length);
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

		att.reset(context->getAttachment(status));
		tra.reset(context->getTransaction(status));

		blob.reset(att->openBlob(status, tra, &in->txt, 0, nullptr));
		// резервируем буфер в двое больше, чем порция которой читаем
		vBuffer.reserve(2 * BUFFER_SIZE);
		auto length = readBlobData(status, blob, BUFFER_SIZE, vBuffer.data());
		if (length > 0) {
			str = std::string_view(vBuffer.data(), length);
		}
		else {
			blob->close(status);
			blob.release();
			stopFlag = true;
		}
	}
}

AutoRelease<IAttachment> att;
AutoRelease<ITransaction> tra;
AutoRelease<IBlob> blob;

std::string_view str;
std::string_view delim;
std::vector<char> vBuffer;
size_t next = 0;
bool stopFlag = false;

FB_UDR_FETCH_PROCEDURE
{
	if (stopFlag)
	{
		return false;
	}
	out->txtNull = FB_FALSE;

	if (next = str.find(delim); next != std::string::npos) {
		// пока находим в строке разделитель
		// возвращаем строки между разделителями		
		if (next > MAX_VARCHAR_SIZE) {
			ISC_STATUS statusVector[] = {
				 isc_arg_gds, isc_random,
				 isc_arg_string, (ISC_STATUS)"Output buffer overflow",
				 isc_arg_end
			};
			throw Firebird::FbException(status, statusVector);
		}
		out->txt.length = static_cast<ISC_USHORT>(next);
		str.copy(out->txt.str, out->txt.length);
		str.remove_prefix(next + delim.length());
		return true;
	}
	// строка после последнего разделителя не обязательно полная,
	// разделитель может быть в не прочитанной части BLOB
	if (blob.hasData()) {
		// копируем в буфер остаток строки
		str.copy(vBuffer.data(), str.length(), 0);

		// читаем следующие ~32Kбайт
		auto length = readBlobData(status, blob, BUFFER_SIZE, vBuffer.data() + str.length());
		if (length > 0) {
			str = std::string_view(vBuffer.data(), str.length() + length);
		}
		else {
			blob->close(status);
			blob.release();
			stopFlag = true;
		}
		
		// ищем разделитель
		next = str.find(delim);
		if (next == std::string::npos)
			next = str.length();
	}
	else {
		next = str.length();
	}
	if (next > MAX_VARCHAR_SIZE) {
		ISC_STATUS statusVector[] = {
			 isc_arg_gds, isc_random,
			 isc_arg_string, (ISC_STATUS)"Output buffer overflow",
			 isc_arg_end
		};
		throw Firebird::FbException(status, statusVector);
	}
	out->txt.length = static_cast<ISC_SHORT>(next);
	str.copy(out->txt.str, out->txt.length);
	if (next + delim.length() <= str.length()) {
		str.remove_prefix(next + delim.length());
	}
	else {
		str.remove_prefix(next);
	}

	stopFlag = !blob.hasData() && str.empty();
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
	// ищем первый из символов separators в строке str начиная с позиции prev
	while ((next = str.find_first_of(separators, prev)) != std::string::npos) {
		// пока находим в строке разделитель
		// возвращаем строки между разделителями	
		const size_t length = next - prev;
		// если строка получилась пустой ищем следующий разделитель
		if (length == 0) {
			prev = next + 1;
			continue;
		}
		// копируем результат в выходное сообщение
		out->txt.length = static_cast<ISC_SHORT>(length);
		str.copy(out->txt.str, out->txt.length, prev);

		prev = next + 1;
		return true;
	}
	// ни одного разделителя не найдено, 
	// словом является строка от предыдущего разделителя и до конца строки str
	next = str.length();

	const size_t length = next - prev;
	// если строка получилась пустой значит результат не возвращаем
	if (length == 0) {
		return false;
	}
	// копируем результат в выходное сообщение
	out->txt.length = static_cast<ISC_SHORT>(length);
	str.copy(out->txt.str, out->txt.length, prev);
	prev = next + 1;
	// инициализация флага остановки
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
		// читаем первые ~32Kбайт
		std::vector<char> vBuffer(BUFFER_SIZE);
		{
			char* buffer = vBuffer.data();
			for (size_t n = 0; !eof && n < BUFFER_SIZE; ) {
				unsigned int l = 0;
				switch (blob->getSegment(status, BUFFER_SIZE, buffer, &l))
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
	// ищем первый из символов separators в строке str начиная с позиции prev
	while ((next = str.find_first_of(separators, prev)) != std::string::npos) {
		// пока находим в строке разделитель
		// возвращаем строки между разделителями	
		const size_t length = next - prev;
		// если строка получилась пустой ищем следующий разделитель
		if (length == 0) {
			prev = next + 1;
			continue;
		}
		if (length > MAX_VARCHAR_SIZE) {
			ISC_STATUS statusVector[] = {
				 isc_arg_gds, isc_random,
				 isc_arg_string, (ISC_STATUS)"Output buffer overflow",
				 isc_arg_end
			};
			throw Firebird::FbException(status, statusVector);
		}
		// копируем результат в выходное сообщение
		out->txt.length = static_cast<ISC_SHORT>(length);
		str.copy(out->txt.str, out->txt.length, prev);
		
		prev = next + 1;
		return true;
	}
	// строка после последнего разделителя не обязательно полная,
	// разделитель может быть в не прочитанной части BLOB
	if (!eof) {
		// если BLOB прочитан не полностью, то
		// читаем следующие ~32Kбайт
		std::string s;
		std::vector<char> vBuffer(BUFFER_SIZE);
		{
			char* buffer = vBuffer.data();
			for (size_t n = 0; !eof && n < BUFFER_SIZE; ) {
				unsigned int l = 0;
				switch (blob->getSegment(status, BUFFER_SIZE, buffer, &l))
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
		// удаляем из строки всё кроме части
		// после последнего разделителя
		str.erase(0, prev);
		prev = 0;
		// и добавляем в неё прочитанной из BLOB
		str.append(s);

		// ищем первый из символов separators в строке str начиная с позиции prev
		next = str.find_first_of(separators, prev);
		if (next == std::string::npos)
			next = str.length();
	}
	else {
		next = str.length();
	}
	while (true) {
		const size_t length = next - prev;
		// если строка получилась пустой ищем следующий разделитель
		if (length == 0) {
			prev = next + 1;
			next = str.find_first_of(separators, prev);
			if (next != std::string::npos)
				continue;
			else
				return false;
		}
		if (length > MAX_VARCHAR_SIZE) {
			ISC_STATUS statusVector[] = {
				 isc_arg_gds, isc_random,
				 isc_arg_string, (ISC_STATUS)"Output buffer overflow",
				 isc_arg_end
			};
			throw Firebird::FbException(status, statusVector);
		}
		// копируем результат в выходное сообщение
		out->txt.length = static_cast<ISC_SHORT>(length);
		str.copy(out->txt.str, out->txt.length, prev);
		prev = next + 1;
		// инициализация флага остановки
		stopFlag = eof && prev >= str.length();
		return true;
	}
}
FB_UDR_END_PROCEDURE

FB_UDR_IMPLEMENT_ENTRY_POINT