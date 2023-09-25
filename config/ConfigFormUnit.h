//---------------------------------------------------------------------------

#ifndef ConfigFormUnitH
#define ConfigFormUnitH
//---------------------------------------------------------------------------
#include <System.Classes.hpp>
#include <Vcl.Controls.hpp>
#include <Vcl.StdCtrls.hpp>
#include <Vcl.Forms.hpp>
#include <Vcl.ComCtrls.hpp>
#include <Vcl.ExtCtrls.hpp>
#include <Vcl.WinXCtrls.hpp>
#include <Vcl.Buttons.hpp>
#include <Vcl.Imaging.pngimage.hpp>
//---------------------------------------------------------------------------
class TConfigForm : public TForm
{
__published:	// Von der IDE verwaltete Komponenten
	TPanel *MenuPnl;
	TPanel *DisplayPnl;
	TSpeedButton *DisplayBtn;
	TSpeedButton *AdvancedBtn;
	TSpeedButton *CompatibilityBtn;
	TPanel *AdvancedPnl;
	TPanel *CompatibilityPnl;
	TComboBox *PresentationCbx;
	TLabel *PresentationLbl;
	TLabel *MaintasLbl;
	TToggleSwitch *MaintasChk;
	TLabel *VsyncLbl;
	TToggleSwitch *VsyncChk;
	TLabel *AdjmouseLbl;
	TToggleSwitch *AdjmouseChk;
	TLabel *DevmodeLbl;
	TToggleSwitch *DevmodeChk;
	TComboBox *RendererCbx;
	TLabel *RendererLbl;
	TLabel *BorderLbl;
	TToggleSwitch *BorderChk;
	TLabel *SavesettingsLbl;
	TToggleSwitch *SavesettingsChk;
	TComboBox *ShaderCbx;
	TLabel *ShaderLbl;
	TLabel *MaxfpsLbl;
	TToggleSwitch *MaxfpsChk;
	TLabel *BoxingLbl;
	TToggleSwitch *BoxingChk;
	TComboBox *MaxgameticksCbx;
	TLabel *MaxgameticksLbl;
	TLabel *NoactivateappLbl;
	TToggleSwitch *NoactivateappChk;
	TLabel *ResolutionsLbl;
	TToggleSwitch *ResolutionsChk;
	TLabel *MinfpsLbl;
	TToggleSwitch *MinfpsChk;
	TToggleSwitch *SinglecpuChk;
	TLabel *SinglecpuLbl;
	TLabel *NonexclusiveLbl;
	TToggleSwitch *NonexclusiveChk;
	TPaintBox *PresentationPbox;
	TPaintBox *RendererPbox;
	TPaintBox *ShaderPbox;
	TPaintBox *MaxgameticksPbox;
	TImage *LanguageImg;
	TPanel *HotkeyPnl;
	TLabel *ToggleWindowedLbl;
	TSpeedButton *HotkeyBtn;
	TEdit *ToggleWindowedEdt;
	TLabel *ToggleWindowedKeyLbl;
	TLabel *MaximizeWindowLbl;
	TEdit *MaximizeWindowEdt;
	TLabel *MaximizeWindowKeyLbl;
	TLabel *UnlockCursor1Lbl;
	TEdit *UnlockCursor1Edt;
	TLabel *UnlockCursor1KeyLbl;
	TLabel *UnlockCursor2Lbl;
	TEdit *UnlockCursor2Edt;
	TLabel *UnlockCursor2KeyLbl;
	TLabel *ScreenshotLbl;
	TEdit *ScreenshotEdt;
	TComboBox *ShaderD3DCbx;
	TSpeedButton *RestoreDefaultsBtn;
	TPanel *ThemePnl;
	TComboBox *ResolutionCbx;
	TLabel *ResolutionLbl;
	TPaintBox *ResolutionPbox;
	void __fastcall DisplayBtnClick(TObject *Sender);
	void __fastcall AdvancedBtnClick(TObject *Sender);
	void __fastcall CompatibilityBtnClick(TObject *Sender);
	void __fastcall FormCreate(TObject *Sender);
	void __fastcall PresentationCbxChange(TObject *Sender);
	void __fastcall MaintasChkClick(TObject *Sender);
	void __fastcall VsyncChkClick(TObject *Sender);
	void __fastcall AdjmouseChkClick(TObject *Sender);
	void __fastcall DevmodeChkClick(TObject *Sender);
	void __fastcall RendererCbxChange(TObject *Sender);
	void __fastcall ShaderCbxChange(TObject *Sender);
	void __fastcall MaxfpsChkClick(TObject *Sender);
	void __fastcall BoxingChkClick(TObject *Sender);
	void __fastcall BorderChkClick(TObject *Sender);
	void __fastcall SavesettingsChkClick(TObject *Sender);
	void __fastcall MaxgameticksCbxChange(TObject *Sender);
	void __fastcall NoactivateappChkClick(TObject *Sender);
	void __fastcall ResolutionsChkClick(TObject *Sender);
	void __fastcall MinfpsChkClick(TObject *Sender);
	void __fastcall SinglecpuChkClick(TObject *Sender);
	void __fastcall NonexclusiveChkClick(TObject *Sender);
	void __fastcall PboxPaint(TObject *Sender);
	void __fastcall LanguageImgClick(TObject *Sender);
	void __fastcall FormActivate(TObject *Sender);
	void __fastcall HotkeyBtnClick(TObject *Sender);
	void __fastcall HotkeyEdtKeyDown(TObject *Sender, WORD &Key, TShiftState Shift);
	void __fastcall HotkeyEdtKeyUp(TObject *Sender, WORD &Key, TShiftState Shift);
	void __fastcall ShaderD3DCbxChange(TObject *Sender);
	void __fastcall RestoreDefaultsBtnClick(TObject *Sender);
	void __fastcall ThemePnlClick(TObject *Sender);



private:	// Benutzer-Deklarationen
	virtual void __fastcall CreateParams(TCreateParams & Params);
	void SaveSettings();
	bool GetBool(TIniFile *ini, System::UnicodeString key, bool defValue);
	void ApplyTranslation(TIniFile *ini);
	System::UnicodeString GetKeyText(WORD key);
	WORD GetKeyCode(System::UnicodeString key);
	WORD TextToKey(System::UnicodeString Text);
	System::UnicodeString KeyToText(WORD key);
public:		// Benutzer-Deklarationen
	__fastcall TConfigForm(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TConfigForm *ConfigForm;
//---------------------------------------------------------------------------
#endif
