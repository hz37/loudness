//---------------------------------------------------------------------------

#ifndef LoudnessUnitH
#define LoudnessUnitH
//---------------------------------------------------------------------------
#include <System.Classes.hpp>
#include <Vcl.Controls.hpp>
#include <Vcl.StdCtrls.hpp>
#include <Vcl.Forms.hpp>

#include <map>


//---------------------------------------------------------------------------
class TForm1 : public TForm
{
__published:	// IDE-managed Components
	TMemo *Memo1;
private:	// User declarations
	bool Execute(const AnsiString Command);
	void ParseJSON(std::map <AnsiString, AnsiString>& Dictionary, AnsiString Text);

	void __fastcall WmDropFiles(TWMDropFiles& Message);

	void Process(const AnsiString InputFile);

	int ChannelCount(const AnsiString Filename);
	void MemoPrint(const AnsiString t, bool Clear=false);

	AnsiString FFMPEG;
public:		// User declarations
	__fastcall TForm1(TComponent* Owner);


		// Message map to intercept Windows messages.

	BEGIN_MESSAGE_MAP
		MESSAGE_HANDLER(WM_DROPFILES, TWMDropFiles, WmDropFiles)
	END_MESSAGE_MAP(TForm)
};
//---------------------------------------------------------------------------
extern PACKAGE TForm1 *Form1;
//---------------------------------------------------------------------------
#endif
