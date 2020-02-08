

#include "SplitUdr.h"
#include <sstream>

using namespace Firebird;

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
		// читаем первые ~32Kбайт
		std::stringstream ss("");
		for (int n = 0; !eof && n < 32765; ) {
			char buffer[32765];
			unsigned int l = 0;
			switch (blob->getSegment(status, sizeof(buffer), &buffer[0], &l))
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
		out->txt.length = next - prev;
		str.copy(out->txt.str, out->txt.length, prev);
		prev = next + delta;
		return true;
	}
	// строка после последнего разделителя не обязательно полная,
	// разделитель модет быть в не прочитанной части BLOB
	if (!eof) {
		// если BLOB прочитан не полностью, то
		// читаем следующие ~32Kбайт
		std::stringstream ss("");
		for (int n = 0; !eof && n < 32765; ) {
			char buffer[32765];
			unsigned int l = 0;
			switch (blob->getSegment(status, sizeof(buffer), &buffer[0], &l))
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
	out->txt.length = next - prev;
	str.copy(out->txt.str, out->txt.length, prev);
	prev = next + delta;
	stopFlag = eof && prev >= str.length();
	return true;
}
FB_UDR_END_PROCEDURE

FB_UDR_IMPLEMENT_ENTRY_POINT