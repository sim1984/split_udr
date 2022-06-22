

#include "SplitUdr.h"
#include <sstream>
#include <memory>

using namespace Firebird;

static const size_t MAX_SEGMENT_SIZE = 65535;
static const size_t MAX_VARCHAR_SIZE = 32765;

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
		str.assign(in->txt.str, in->txt.length);

		delim.assign(in->separator.str, in->separator.length);
		delta = delim.length();
	}
}


std::string str;
std::string delim;
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
		// пока находим в строке разделитель
		// возвращаем строки между разделителями		
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
		delim.assign(in->separator.str, in->separator.length);
		delta = delim.length();

		att.reset(context->getAttachment(status));
		tra.reset(context->getTransaction(status));

		//const unsigned char bpb[] = { isc_bpb_version1,  isc_bpb_type, 1, isc_bpb_type_stream };
		blob.reset(att->openBlob(status, tra, &in->txt, 0, nullptr));
		std::stringstream ss("");
		// читаем первые ~32Kбайт
		auto b = std::make_unique<char[]>(MAX_VARCHAR_SIZE + 1);
		{
			char* buffer = b.get();
			for (size_t n = 0; !eof && n < MAX_VARCHAR_SIZE; ) {
				unsigned int l = 0;
				switch (blob->getSegment(status, MAX_VARCHAR_SIZE, buffer, &l))
				{
				case IStatus::RESULT_OK:
				case IStatus::RESULT_SEGMENT:
					ss.write(buffer, l);
					n += l;
					continue;
				default:
					blob->close(status);
					eof = true;
					break;
				}
			}
		}
		str = ss.str();
	}
}


AutoRelease<IAttachment> att;
AutoRelease<ITransaction> tra;
AutoRelease<IBlob> blob;


std::string str;
std::string delim;
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
		// пока находим в строке разделитель
		// возвращаем строки между разделителями		
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
	// строка после последнего разделителя не обязательно полная,
	// разделитель может быть в не прочитанной части BLOB
	if (!eof) {
		// если BLOB прочитан не полностью, то
		// читаем следующие ~32Kбайт
		std::stringstream ss("");
		auto b = std::make_unique<char[]>(MAX_VARCHAR_SIZE + 1);
		{
			char* buffer = b.get();
			for (size_t n = 0; !eof && n < MAX_VARCHAR_SIZE; ) {
				unsigned int l = 0;
				switch (blob->getSegment(status, MAX_VARCHAR_SIZE, buffer, &l))
				{
				case IStatus::RESULT_OK:
				case IStatus::RESULT_SEGMENT:
					ss.write(buffer, l);
					n += l;
					continue;
				default:
					blob->close(status);
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
		str.append(ss.str());
		
		// ищем разделитель
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
		str.assign(in->txt.str, in->txt.length);

		if (in->separatorsNull) {
			separators = " \n\r\t,.?!:;/\\|<>[]{}()@#$%^&*-+='\"~`";
		}
		else
		   separators.assign(in->separators.str, in->separators.length);

	}
}


std::string str;
std::string separators;
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
			separators = " \n\r\t,.?!:;/\\|<>[]{}()@#$%^&*-+='\"~`";
		}
		else
		   separators.assign(in->separators.str, in->separators.length);

		att.reset(context->getAttachment(status));
		tra.reset(context->getTransaction(status));

		blob.reset(att->openBlob(status, tra, &in->txt, 0, nullptr));
		// читаем первые ~32Kбайт
		std::stringstream ss("");
		auto b = std::make_unique<char[]>(MAX_VARCHAR_SIZE + 1);
		{
			char* buffer = b.get();
			for (size_t n = 0; !eof && n < MAX_VARCHAR_SIZE; ) {
				unsigned int l = 0;
				switch (blob->getSegment(status, MAX_VARCHAR_SIZE, buffer, &l))
				{
				case IStatus::RESULT_OK:
				case IStatus::RESULT_SEGMENT:
					ss.write(buffer, l);
					n += l;
					continue;
				default:
					blob->close(status);
					eof = true;
					break;
				}
			}
		}
		str = ss.str();
	}
}


AutoRelease<IAttachment> att;
AutoRelease<ITransaction> tra;
AutoRelease<IBlob> blob;


std::string str;
std::string separators;
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
		if (length > 32765) {
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
		std::stringstream ss("");
		auto b = std::make_unique<char[]>(MAX_VARCHAR_SIZE + 1);
		{
			char* buffer = b.get();
			for (size_t n = 0; !eof && n < MAX_VARCHAR_SIZE; ) {
				unsigned int l = 0;
				switch (blob->getSegment(status, MAX_VARCHAR_SIZE, buffer, &l))
				{
				case IStatus::RESULT_OK:
				case IStatus::RESULT_SEGMENT:
					ss.write(buffer, l);
					n += l;
					continue;
				default:
					blob->close(status);
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
		str.append(ss.str());

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
		if (length > 32765) {
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