//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include <shellapi.h>
#include <pcre.h>
#include <memory>

#include "sndfile.h"

#include "LoudnessUnit.h"

//---------------------------------------------------------------------------

#pragma package(smart_init)
#pragma resource "*.dfm"

TForm1 *Form1;

const int CLoudness = -23; // LUFS
const int CTruePeak = -1; // dBTP
const int CLoudnessRange = 10; // LU
const int CStereo = 2; //channels


// #define CONSOLE_DEBUG

//---------------------------------------------------------------------------

__fastcall TForm1::TForm1(TComponent* Owner)
	: TForm(Owner)
{

	// This program allows for dropped files.

	DragAcceptFiles(Handle, true);

	AnsiString ExeDir = ExtractFilePath(Application->ExeName);

	FFMPEG = IncludeTrailingBackslash(ExeDir) + "bin\\ffmpeg.exe";

	if(!FileExists(FFMPEG))
	{
		ShowMessage("ffmpeg not found. This is a problem.");
	}

#ifdef CONSOLE_DEBUG
	::AllocConsole();
	::AttachConsole(GetCurrentProcessId());
	freopen("CONOUT$", "w", stdout);

    MemoPrint("Running in console debug mode");
#endif

}
//---------------------------------------------------------------------------

void TForm1::Process(const AnsiString InputFile)
{
	if(ExtractFileExt(InputFile).LowerCase() != ".wav")
	{
		MemoPrint("Skipping " + InputFile + " => not a *.wav file");
		return;
	}

	MemoPrint("Processing: " + InputFile);

	MemoPrint("Pass 1: scanning " + InputFile, true);

	AnsiString FFMPEGCommand = AnsiString().sprintf
	(
		"\"%s\" -i \"%s\" -af loudnorm=I=%d:TP=%d:LRA=%d:linear=true:print_format=json -f null -",
		FFMPEG.c_str(),
		InputFile.c_str(),
		CLoudness,
		CTruePeak,
		CLoudnessRange
	);


	if(Execute(FFMPEGCommand))
	{
		// The OutputMemo contains a JSON block;

		std::map <AnsiString, AnsiString> Dictionary;
		ParseJSON(Dictionary, Memo1->Lines->CommaText);

		// Build ffmpeg string for pass 2 with obtained parameters.

		AnsiString OutputFile = ChangeFileExt(InputFile, "_R128.wav");

		FFMPEGCommand = AnsiString().sprintf
		(
			"\"%s\" -y -i \"%s\" -af loudnorm=I=%d:TP=%d:LRA=%d:measured_I=%s:measured_LRA=%s:measured_TP=%s:measured_thresh=%s:offset=%s:linear=true:print_format=summary -ar 48k -acodec pcm_s24le \"%s\"",
			FFMPEG.c_str(),
			InputFile.c_str(),
			CLoudness,
			CTruePeak,
			CLoudnessRange,
			Dictionary["input_i"].c_str(),
			Dictionary["input_lra"].c_str(),
			Dictionary["input_tp"].c_str(),
			Dictionary["output_thresh"].c_str(),
			Dictionary["target_offset"].c_str(),
			OutputFile.c_str()
		);

		MemoPrint("Pass 2: linear gain change " + InputFile);
		MemoPrint(FFMPEGCommand);
		Execute(FFMPEGCommand);
		MemoPrint("Output written to: " + OutputFile, true);
	}

	if(ChannelCount(InputFile) != CStereo)
	{
		MemoPrint(InputFile + "was not 2 channel. Performing fold down.");


		// Build ffmpeg string for pass 3; in between fold down at wrong loudness.

		AnsiString TempOutputFile = ChangeFileExt(InputFile, "_folddown.wav");
		AnsiString OutputFile = ChangeFileExt(InputFile, "_folddown_R128.wav");

		FFMPEGCommand = AnsiString().sprintf
		(
			"\"%s\" -y -i \"%s\" -ac 2 \"%s\"",
			FFMPEG.c_str(),
			InputFile.c_str(),
			TempOutputFile.c_str()
		);

		MemoPrint("Pass 3: fold down of " + InputFile);
		MemoPrint(FFMPEGCommand);

		Execute(FFMPEGCommand);
		MemoPrint("Output written to: " + TempOutputFile, true);


		MemoPrint("Pass 4: scanning " + TempOutputFile, true);

		AnsiString FFMPEGCommand = AnsiString().sprintf
		(
			"\"%s\" -i \"%s\" -af loudnorm=I=%d:TP=%d:LRA=%d:linear=true:print_format=json -f null -",
			FFMPEG.c_str(),
			TempOutputFile.c_str(),
			CLoudness,
			CTruePeak,
			CLoudnessRange
		);

		if(Execute(FFMPEGCommand))
		{
			// The OutputMemo contains a JSON block;

			std::map <AnsiString, AnsiString> Dictionary;
			ParseJSON(Dictionary, Memo1->Lines->CommaText);

			// Build ffmpeg string for pass 5 with obtained parameters.

			FFMPEGCommand = AnsiString().sprintf
			(
				"\"%s\" -y -i \"%s\" -af loudnorm=I=%d:TP=%d:LRA=%d:measured_I=%s:measured_LRA=%s:measured_TP=%s:measured_thresh=%s:offset=%s:linear=true:print_format=summary -ar 48k -acodec pcm_s24le \"%s\"",
				FFMPEG.c_str(),
				TempOutputFile.c_str(),
				CLoudness,
				CTruePeak,
				CLoudnessRange,
				Dictionary["input_i"].c_str(),
				Dictionary["input_lra"].c_str(),
				Dictionary["input_tp"].c_str(),
				Dictionary["output_thresh"].c_str(),
				Dictionary["target_offset"].c_str(),
				OutputFile.c_str()
			);


			MemoPrint("Pass 5: linear gain change " + InputFile);
			MemoPrint(FFMPEGCommand);
			Execute(FFMPEGCommand);
			MemoPrint("Output written to: " + OutputFile, true);

			// Remove tempfile.

			DeleteFile(TempOutputFile);
		}

	}


}


void TForm1::MemoPrint(const AnsiString t, bool Clear)
{
	if(Clear)
	{
		Memo1->Text = t;
	}
	else
	{
		Memo1->Lines->Add(t);
	}

	#ifdef CONSOLE_DEBUG
		std::cout << t.c_str() << std::endl;;
	#endif
 }


//---------------------------------------------------------------------------

// Low level Windows event when one or more files are dropped onto our form.

void __fastcall TForm1::WmDropFiles(TWMDropFiles& Message)
{
	Screen->Cursor = crHourGlass;

	HDROP DropHandle = reinterpret_cast <HDROP> (Message.Drop);

	const int FileCount = DragQueryFileA
	(
		reinterpret_cast <HDROP> (Message.Drop),
		0xFFFFFFFF,
		0,
		0
	);

	// Process files.

	for(int Idx = 0; Idx < FileCount; ++Idx)
	{
		char Buffer[MAX_PATH];

		DragQueryFileA(DropHandle, Idx, Buffer, sizeof(Buffer));
		Process(Buffer);
	}

	DragFinish(DropHandle);

	Screen->Cursor = crDefault;

	MemoPrint("");

	if(FileCount > 1)
	{
		MemoPrint
		(
			AnsiString().sprintf("Done processing %d files.", FileCount)
		);
	}
	else if(FileCount == 1)
	{
		MemoPrint
		(
			AnsiString().sprintf("Done processing file.")
		);
	}

}

//---------------------------------------------------------------------------

bool TForm1::Execute(const AnsiString Command)
{
	//OutputMemo->Text = Command;

	Application->ProcessMessages();

	// Create pipe for the console stdout.

	SECURITY_ATTRIBUTES sa;

	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = true;
	sa.lpSecurityDescriptor = 0;

	HANDLE ReadPipeHandle;
	HANDLE WritePipeHandle;

	if (!CreatePipe(&ReadPipeHandle, &WritePipeHandle, &sa, 0))
	{
		return false;
	}

	// Create a console.

	STARTUPINFO si;

	ZeroMemory(&si, sizeof(STARTUPINFO));

	si.cb = sizeof(STARTUPINFO);
	si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
	si.wShowWindow = SW_HIDE;
	si.hStdOutput = WritePipeHandle;
	si.hStdError = WritePipeHandle;

	PROCESS_INFORMATION pi;

	ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));

	int CmdBufferLen = Command.WideCharBufSize();
	wchar_t* cmd = new wchar_t[CmdBufferLen];
	int ExeBufferLen = FFMPEG.WideCharBufSize();
	wchar_t* exe = new wchar_t[ExeBufferLen];

	BOOL result = CreateProcess
	(
		FFMPEG.WideChar(exe, ExeBufferLen),
		Command.WideChar(cmd, CmdBufferLen),
		0,
		0,
		true,
		0,
		0,
		0,
		&si,
		&pi
	);

	delete [] cmd;

	if (!result)
	{
		MemoPrint("Error launching ffmpeg!", true);
		return false;
	}

	// Read from pipe.

	char Data[8192];

	for (;;)
	{
		// Allow UI to be a bit more responsive.

		Application->ProcessMessages();

		DWORD BytesRead;
		DWORD TotalBytes;
		DWORD BytesLeft;

		// Check for the presence of data in the pipe.

		PeekNamedPipe(ReadPipeHandle, Data, sizeof(Data), &BytesRead,
			&TotalBytes, &BytesLeft);

		// If there are bytes, read them

		if (BytesRead) {
			ReadFile(ReadPipeHandle, Data, sizeof(Data) - 1, &BytesRead, 0);
			Data[BytesRead] = 0;

			MemoPrint(AnsiString(Data));

		}
		else {
			// Is the console app terminated?

			if (WaitForSingleObject(pi.hProcess, 0) == WAIT_OBJECT_0) {
				break;
			}

		}
	}

	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	CloseHandle(ReadPipeHandle);
	CloseHandle(WritePipeHandle);

	return true;
}

//---------------------------------------------------------------------------

/*

We get a block like this one.

[Parsed_loudnorm_0 @ 000001c8dcab3920]
{
	"input_i" : "-23.23",
	"input_tp" : "-7.05",
	"input_lra" : "2.80",
	"input_thresh" : "-33.67",
	"output_i" : "-22.85",
	"output_tp" : "-6.83",
	"output_lra" : "1.00",
	"output_thresh" : "-33.24",
	"normalization_type" : "dynamic",
	"target_offset" : "-0.15"
}
*/

void TForm1::ParseJSON
(
	std::map <AnsiString, AnsiString>& Dictionary,
	AnsiString Text /* CSV ! */
)
{
	std::auto_ptr<TStringList> StringList(new TStringList(0));
	StringList.get()->CommaText = Text;

	const AnsiString Pattern = "^\\s*\"(\\S+)\" : \"(\\S+)\".*$";

	const int CompileOptions = PCRE_ANCHORED | PCRE_DOLLAR_ENDONLY;
	const char* ErrorPointer = 0;
	int ErrorOffset          = 0;

	const unsigned char* TablePointer = pcre_maketables();

	pcre* CompiledPattern = pcre_compile
	(
		Pattern.c_str(),
		CompileOptions,
		&ErrorPointer,
		&ErrorOffset,
		TablePointer
	);

	const int ExecOptions = 0;

	// n == 2, so (n + 1) * 3 == 9

	const int MatchVectorSize = 9;

	int MatchVector[MatchVectorSize];

	int LineCount = StringList.get()->Count;

	for(int Idx = 0; Idx < LineCount; ++Idx)
	{
		AnsiString Line = StringList->Strings[Idx];

		int ExecResult = pcre_exec
		(
			CompiledPattern,
			0,
			Line.c_str(),
			Line.Length(),
			ExecOptions,
			MatchVector,
			MatchVectorSize
		);

		bool Matched;

		switch(ExecResult)
		{
			case PCRE_ERROR_NOMATCH:
			case PCRE_ERROR_NULL:
			case PCRE_ERROR_BADOPTION:
			case PCRE_ERROR_BADMAGIC:
			case PCRE_ERROR_UNKNOWN_NODE:
			case PCRE_ERROR_NOMEMORY:
				Matched = false;
				break;
			default:
				Matched = true;
		}

		if(Matched)
		{
			AnsiString Key = Line.SubString
			(
				MatchVector[2] + 1,
				MatchVector[3] - MatchVector[2]
			);

			AnsiString Value = Line.SubString
			(
				MatchVector[4] + 1,
				MatchVector[5] - MatchVector[4]
			);

			Dictionary[Key] = Value;
		}
	}

	free(CompiledPattern);

}

//---------------------------------------------------------------------------

// We do fold down of multichannel audio to 2 channels,
// so we need to figure out if input happens to be multichannel.

int TForm1::ChannelCount(const AnsiString Filename)
{
	// Attempt to open the input file.

	SF_INFO FileInfo;
	memset(&FileInfo, 0, sizeof(FileInfo));

	SNDFILE* InputFile = sf_open(Filename.c_str(), SFM_READ, &FileInfo);

	if(!InputFile)
	{
		throw Exception("Error opening " + Filename);
	}

	sf_close(InputFile);

	return FileInfo.channels;
}

//---------------------------------------------------------------------------



