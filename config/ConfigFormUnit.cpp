//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include <IniFiles.hpp>
#include <StrUtils.hpp>
#include <IOUtils.hpp>
#include <SysUtils.hpp>
#include <Registry.hpp>
#include <System.Hash.hpp>
#include "ConfigFormUnit.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TConfigForm *ConfigForm;
bool Initialized;
bool IsEnglish;
System::UnicodeString KeyNames[256];

/* Save previous settings so we don't override custom settings */
int Maxfps;
int Savesettings;
int Resolutions;
int Minfps;

#define GAME_PATH (TPath::GetDirectoryName(Application->ExeName) + "\\")

//---------------------------------------------------------------------------
__fastcall TConfigForm::TConfigForm(TComponent* Owner)
	: TForm(Owner)
{
}

void __fastcall TConfigForm::CreateParams(TCreateParams & Params)
{
	TForm::CreateParams(Params);

	StrCopy(
		Params.WinClassName,
		THashSHA1::GetHashString(Application->ExeName).w_str());
}

void __fastcall TConfigForm::LanguageImgClick(TObject *Sender)
{
	auto *ini = new TIniFile(GAME_PATH + "ddraw.ini");
	ini->WriteString("ddraw", "configlang", IsEnglish ? "auto" : "english");
	delete ini;

	ShellExecute(
		NULL,
		L"open",
		Application->ExeName.w_str(),
		L"-restart",
		NULL,
		SW_SHOWNORMAL);

	Application->Terminate();
}

void __fastcall TConfigForm::ThemePnlClick(TObject *Sender)
{
	auto *ini = new TIniFile(GAME_PATH + "ddraw.ini");
	auto theme =
		ThemePnl->Color == (TColor)RGB(31, 31, 31) ? "Cobalt XEMedia" : "Windows10";

	ini->WriteString("ddraw", "configtheme", theme);

	delete ini;

	ShellExecute(
		NULL,
		L"open",
		Application->ExeName.w_str(),
		L"-restart",
		NULL,
		SW_SHOWNORMAL);

	Application->Terminate();
}

void __fastcall TConfigForm::RestoreDefaultsBtnClick(TObject *Sender)
{
	if (Application->MessageBox(
			(RestoreDefaultsBtn->Caption + "?").w_str(),
			L"cnc-ddraw",
			MB_YESNO) == IDNO) {

		return;
	}

	auto *ini = new TIniFile(GAME_PATH + "Warcraft II BNE.ini");

	ini->WriteString("Game", "Resolution", "4");
	ini->WriteString("Game", "Width", "0");
	ini->WriteString("Game", "Height", "0");

	delete ini;

	DeleteFile(GAME_PATH + "ddraw.ini");

	ShellExecute(
		NULL,
		L"open",
		Application->ExeName.w_str(),
		L"-restart",
		NULL,
		SW_SHOWNORMAL);

	Application->Terminate();
}

void TConfigForm::ApplyTranslation(TIniFile *ini)
{
	auto lang = LowerCase(ini->ReadString("ddraw", "configlang", "auto"));
	int priID = SysLocale.PriLangID;

	if (lang == "chinese" || (lang == "auto" && priID == LANG_CHINESE)) {
		LanguageImg->Visible = true;

		/* -Chinese Simplified- made by universal963 @ github */

		ConfigForm->Caption = L"cnc-ddraw 配置";
		DisplayBtn->Caption = L"显示设置";
		AdvancedBtn->Caption = L"高级设置";
		HotkeyBtn->Caption = L"热键设置";
		CompatibilityBtn->Caption = L"兼容性设置";
		RestoreDefaultsBtn->Caption = L"恢复默认设置";
		PresentationLbl->Caption = L"显示方式";
		ResolutionLbl->Caption = L"解决";	
		MaintasLbl->Caption = L"保持纵横比";
		VsyncLbl->Caption = L"打开垂直同步";
		AdjmouseLbl->Caption = L"调整鼠标灵敏度";
		DevmodeLbl->Caption = L"锁定光标到窗口/屏幕";
		RendererLbl->Caption = L"渲染器";
		BorderLbl->Caption = L"在窗口模式下显示窗口边框";
		SavesettingsLbl->Caption = L"记住窗口位置和大小";
		ShaderLbl->Caption = L"OpenGL着色器";
		MaxfpsLbl->Caption = L"限制帧率";
		BoxingLbl->Caption = L"打开窗盒显示/整数缩放";
		ToggleWindowedLbl->Caption = L"切换窗口模式";
		MaximizeWindowLbl->Caption = L"最大化窗口";
		UnlockCursor1Lbl->Caption = L"解锁光标 1";
		UnlockCursor2Lbl->Caption = L"解锁光标 2";
		ScreenshotLbl->Caption = L"截屏";
		MaxgameticksLbl->Caption = L"限制游戏速率";
		NoactivateappLbl->Caption = L"修复损坏的Alt+Tab功能";
		ResolutionsLbl->Caption = L"解锁其他屏幕分辨率";
		MinfpsLbl->Caption = L"强制高FPS / 修复使用Freesync/G-Sync的卡顿问题";
		SinglecpuLbl->Caption = L"修复性能不佳和声音问题";
		NonexclusiveLbl->Caption = L"修复不显示的视频/UI元素";

		RendererCbx->Items->Clear();
		RendererCbx->AddItem(L"自动", NULL);
		RendererCbx->AddItem(L"Direct3D 9", NULL);
		RendererCbx->AddItem(L"OpenGL", NULL);
		RendererCbx->AddItem(L"GDI", NULL);

		PresentationCbx->Items->Clear();
		PresentationCbx->AddItem(L"全屏", NULL);
		PresentationCbx->AddItem(L"拉伸至全屏", NULL);
		PresentationCbx->AddItem(L"无边框", NULL);
		PresentationCbx->AddItem(L"窗口化", NULL);

		MaxgameticksCbx->Items->Clear();
		MaxgameticksCbx->AddItem(L"无限制", NULL);
		MaxgameticksCbx->AddItem(L"与显示器刷新率同步", NULL);
		MaxgameticksCbx->AddItem(L"模拟60hz刷新率显示器", NULL);
		MaxgameticksCbx->AddItem(L"1000tick每秒", NULL);
		MaxgameticksCbx->AddItem(L"500tick每秒", NULL);
		MaxgameticksCbx->AddItem(L"250tick每秒", NULL);
		MaxgameticksCbx->AddItem(L"125tick每秒", NULL);
		MaxgameticksCbx->AddItem(L"60tick每秒", NULL);
		MaxgameticksCbx->AddItem(L"30tick每秒", NULL);
		MaxgameticksCbx->AddItem(L"25tick每秒", NULL);
		MaxgameticksCbx->AddItem(L"15tick每秒", NULL);
	}
	else if (lang == "spanish" || (lang == "auto" && priID == LANG_SPANISH)) {
		LanguageImg->Visible = true;

		/* -Spanish- made by c-sanchez @ github */

		ConfigForm->Caption = L"Ajustes de cnc-ddraw";
		DisplayBtn->Caption = L"Ajustes de pantalla";
		AdvancedBtn->Caption = L"Ajustes avanzados";
		HotkeyBtn->Caption = L"Teclas de acceso rápido";
		CompatibilityBtn->Caption = L"Ajustes de compatibilidad";
		RestoreDefaultsBtn->Caption = L"Restaurar la configuración predeterminada";
		PresentationLbl->Caption = L"Presentación";
		ResolutionLbl->Caption = L"Resolución";
		MaintasLbl->Caption = L"Mantener la relación de aspecto";
		VsyncLbl->Caption = L"Activar VSync";
		AdjmouseLbl->Caption = L"Ajustar sensibilidad de ratón";
		DevmodeLbl->Caption = L"Bloquear cursor a la ventana / pantalla";
		RendererLbl->Caption = L"Renderizador";
		BorderLbl->Caption = L"Mostrar bordes en modo ventana";
		SavesettingsLbl->Caption = L"Recordar posición y tamaño de ventana";
		ShaderLbl->Caption = L"Sombreador OpenGL";
		MaxfpsLbl->Caption = L"Limitar velocidad de fotogramas";
		BoxingLbl->Caption = L"Activar encajado de ventanas / escalado de enteros";
		ToggleWindowedLbl->Caption = L"Alternar modo de ventana";
		MaximizeWindowLbl->Caption = L"Maximizar ventana";
		UnlockCursor1Lbl->Caption = L"Desbloquear cursor 1";
		UnlockCursor2Lbl->Caption = L"Desbloquear cursor 2";
		ScreenshotLbl->Caption = L"Captura de pantalla";
		MaxgameticksLbl->Caption = L"Limitar velocidad de juego";
		NoactivateappLbl->Caption = L"Corregir Alt+Tab roto";
		ResolutionsLbl->Caption = L"Desbloquear resoluciones de pantalla adicionales";
		MinfpsLbl->Caption = L"Forzar un alto FPS / Corregir retrasos en Freesync/G-Sync";
		SinglecpuLbl->Caption = L"Solucione el mal rendimiento y los problemas de sonido";
		NonexclusiveLbl->Caption = L"Corregir vídeos / elementos de interfaz invisibles";

		RendererCbx->Items->Clear();
		RendererCbx->AddItem(L"Automático", NULL);
		RendererCbx->AddItem(L"Direct3D 9", NULL);
		RendererCbx->AddItem(L"OpenGL", NULL);
		RendererCbx->AddItem(L"GDI", NULL);

		PresentationCbx->Items->Clear();
		PresentationCbx->AddItem(L"Pantalla completa", NULL);
		PresentationCbx->AddItem(L"Pantalla completa ampliada", NULL);
		PresentationCbx->AddItem(L"Sin bordes", NULL);
		PresentationCbx->AddItem(L"Ventana", NULL);

		MaxgameticksCbx->Items->Clear();
		MaxgameticksCbx->AddItem(L"Sin límite", NULL);
		MaxgameticksCbx->AddItem(L"Sincronizar con tasa de refresco de monitor", NULL);
		MaxgameticksCbx->AddItem(L"Emular monitor con tasa de refresco de 60hz", NULL);
		MaxgameticksCbx->AddItem(L"1000 tics por segundo", NULL);
		MaxgameticksCbx->AddItem(L"500 tics por segundo", NULL);
		MaxgameticksCbx->AddItem(L"250 tics por segundo", NULL);
		MaxgameticksCbx->AddItem(L"125 tics por segundo", NULL);
		MaxgameticksCbx->AddItem(L"60 tics por segundo", NULL);
		MaxgameticksCbx->AddItem(L"30 tics por segundo", NULL);
		MaxgameticksCbx->AddItem(L"25 tics por segundo", NULL);
		MaxgameticksCbx->AddItem(L"15 tics por segundo", NULL);
	}
	else if (lang == "german" || (lang == "auto" && priID == LANG_GERMAN)) {
		LanguageImg->Visible = true;

		/* -German- made by helgo1506 @ github */

		ConfigForm->Caption = L"cnc-ddraw Konfiguration";
		DisplayBtn->Caption = L"Anzeigeeinstellungen";
		AdvancedBtn->Caption = L"Erweiterte Einstellungen";
		HotkeyBtn->Caption = L"Tastenkürzel-Einstellungen";
		CompatibilityBtn->Caption = L"Kompatibilitätseinstellungen";
		RestoreDefaultsBtn->Caption = L"Standardeinstellungen wiederherstellen";
		PresentationLbl->Caption = L"Darstellung";
		ResolutionLbl->Caption = L"Auflösung";
		MaintasLbl->Caption = L"Seitenverhältnis beibehalten";
		VsyncLbl->Caption = L"VSync aktiveren";
		AdjmouseLbl->Caption = L"Mausempfindlichkeit anpassen";
		DevmodeLbl->Caption = L"Sperre Cursor zu Fenster / Bildschirm"; //Not 100% sure, if not a better translation exists
		RendererLbl->Caption = L"Renderer";
		BorderLbl->Caption = L"Zeige Fensterränder in Fenstermodus";
		SavesettingsLbl->Caption = L"Fensterposition und Größe merken";
		ShaderLbl->Caption = L"OpenGL Shader";
		MaxfpsLbl->Caption = L"Limitiere Aktualisierungsrate";
		BoxingLbl->Caption = L"Fensterboxing / Integer Skalierung aktivieren"; //Not 100% sure if "windowboxing" can be translated better.
		ToggleWindowedLbl->Caption = L"Fenstermodus umschalten";
		MaximizeWindowLbl->Caption = L"Fenster maximieren";
		UnlockCursor1Lbl->Caption = L"Cursor entsperren 1";
		UnlockCursor2Lbl->Caption = L"Cursor entsperren 2";
		ScreenshotLbl->Caption = L"Bildschirmfoto";
		MaxgameticksLbl->Caption = L"Spielgeschwindigkeit limitieren";
		NoactivateappLbl->Caption = L"Fehlerhaftes Alt+Tab reparieren"; //The first word can be ignored if its to long (eng word "Fix"
		ResolutionsLbl->Caption = L"Zusätzliche Bildschirmauflösungen freischalten";
		MinfpsLbl->Caption = L"Erzwinge Hohe FPS / Repariere Stottern bei Freesync/G-Sync";
		SinglecpuLbl->Caption = L"Schlechte Leistung und Soundprobleme reparieren";
		NonexclusiveLbl->Caption = L"Unsichtbare Videos / UI Elemente reparieren";

		RendererCbx->Items->Clear();
		RendererCbx->AddItem(L"Automatisch", NULL);
		RendererCbx->AddItem(L"Direct3D 9", NULL);
		RendererCbx->AddItem(L"OpenGL", NULL);
		RendererCbx->AddItem(L"GDI", NULL);

		PresentationCbx->Items->Clear();
		PresentationCbx->AddItem(L"Vollbild", NULL);
		PresentationCbx->AddItem(L"Hochskaliertes Vollbild", NULL);
		PresentationCbx->AddItem(L"Ränderfreies Fenster", NULL);
		PresentationCbx->AddItem(L"Fenstermodus", NULL);

		MaxgameticksCbx->Items->Clear();
		MaxgameticksCbx->AddItem(L"Unlimitiert", NULL);
		MaxgameticksCbx->AddItem(L"Sync mit Bildschirmaktualisierungsrate", NULL);
		MaxgameticksCbx->AddItem(L"Emuliere 60hz Bildschirmaktualisierungsrate", NULL);
		MaxgameticksCbx->AddItem(L"1000 Ticks pro Sekunde", NULL);
		MaxgameticksCbx->AddItem(L"500 Ticks pro Sekunde", NULL);
		MaxgameticksCbx->AddItem(L"250 Ticks pro Sekunde", NULL);
		MaxgameticksCbx->AddItem(L"125 Ticks pro Sekunde", NULL);
		MaxgameticksCbx->AddItem(L"60 Ticks pro Sekunde", NULL);
		MaxgameticksCbx->AddItem(L"30 Ticks pro Sekunde", NULL);
		MaxgameticksCbx->AddItem(L"25 Ticks pro Sekunde", NULL);
		MaxgameticksCbx->AddItem(L"15 Ticks pro Sekunde", NULL);
	}
	else if (lang == "russian" || (lang == "auto" && priID == LANG_RUSSIAN)) {
		LanguageImg->Visible = true;
		ClientWidth *= 1.13;

		/* -Russian- made by shikulja @ github */

		ConfigForm->Caption = L"Настройки cnc-ddraw";
		DisplayBtn->Caption = L"Настройки отображения";
		AdvancedBtn->Caption = L"Расширенные настройки";
		HotkeyBtn->Caption = L"Настройки горячих клавиш";
		CompatibilityBtn->Caption = L"Настройки совместимости";
		RestoreDefaultsBtn->Caption = L"Восстановить настройки по умолчанию";
		PresentationLbl->Caption = L"Отображение";
		ResolutionLbl->Caption = L"Разрешение";
		MaintasLbl->Caption = L"Сохранять соотношение сторон";
		VsyncLbl->Caption = L"Включить VSync";
		AdjmouseLbl->Caption = L"Регулировка чувствительности мыши";
		DevmodeLbl->Caption = L"Зафиксировать курсор на окне / экране";
		RendererLbl->Caption = L"Рендер";
		BorderLbl->Caption = L"Отображение границ окна в оконном режиме";
		SavesettingsLbl->Caption = L"Запомнить положение и размер окна";
		ShaderLbl->Caption = L"Шейдер OpenGL";
		MaxfpsLbl->Caption = L"Ограничить частоту кадров";
		BoxingLbl->Caption = L"Включить windowboxing / целочисленное масштабирование";
		ToggleWindowedLbl->Caption = L"Переключить оконный режим";
		MaximizeWindowLbl->Caption = L"Развернуть окно";
		UnlockCursor1Lbl->Caption = L"Разблокировать курсор 1";
		UnlockCursor2Lbl->Caption = L"Разблокировать курсор 2";
		ScreenshotLbl->Caption = L"Скриншот";
		MaxgameticksLbl->Caption = L"Ограничить скорость игры";
		NoactivateappLbl->Caption = L"Исправить сломанный Alt+Tab";
		ResolutionsLbl->Caption = L"Разблокировать дополнительные разрешения экрана";
		MinfpsLbl->Caption = L"Принудительно высокий FPS / Исправить заикание при Freesync/G-Sync";
		SinglecpuLbl->Caption = L"Исправление проблем с производительностью и звуком";
		NonexclusiveLbl->Caption = L"Исправить невидимые видео / элементы пользовательского интерфейса";

		RendererCbx->Items->Clear();
		RendererCbx->AddItem(L"Автоматический", NULL);
		RendererCbx->AddItem(L"Direct3D 9", NULL);
		RendererCbx->AddItem(L"OpenGL", NULL);
		RendererCbx->AddItem(L"GDI", NULL);

		PresentationCbx->Items->Clear();
		PresentationCbx->AddItem(L"Полноэкранный", NULL);
		PresentationCbx->AddItem(L"Полноэкранный масштабированный", NULL);
		PresentationCbx->AddItem(L"Без границ", NULL);
		PresentationCbx->AddItem(L"Оконный", NULL);

		MaxgameticksCbx->Items->Clear();
		MaxgameticksCbx->AddItem(L"Без ограничений", NULL);
		MaxgameticksCbx->AddItem(L"Синхронизация с частотой обновления монитора", NULL);
		MaxgameticksCbx->AddItem(L"Эмуляция частоты обновления монитора 60 Гц", NULL);
		MaxgameticksCbx->AddItem(L"1000 тиков в секунду", NULL);
		MaxgameticksCbx->AddItem(L"500 тиков в секунду", NULL);
		MaxgameticksCbx->AddItem(L"250 тиков в секунду", NULL);
		MaxgameticksCbx->AddItem(L"125 тиков в секунду", NULL);
		MaxgameticksCbx->AddItem(L"60 тиков в секунду", NULL);
		MaxgameticksCbx->AddItem(L"30 тиков в секунду", NULL);
		MaxgameticksCbx->AddItem(L"25 тиков в секунду", NULL);
		MaxgameticksCbx->AddItem(L"15 тиков в секунду", NULL);
	}
	else if (lang == "hungarian" || (lang == "auto" && priID == LANG_HUNGARIAN)) {
		LanguageImg->Visible = true;

		/* -Hungarian - made by cyberdyne-sys @ github */

		ConfigForm->Caption = L"cnc-ddraw Beállító";
		DisplayBtn->Caption = L"Képbeállítások";
		AdvancedBtn->Caption = L"Haladó Beállítások";
		HotkeyBtn->Caption = L"Gyorsbillentyűk beállításai";
		CompatibilityBtn->Caption = L"Kompatibilitás Beállítások";
		RestoreDefaultsBtn->Caption = L"Visszaállítja az alapértelmezett beállításokat";
		PresentationLbl->Caption = L"Bemutató";
		ResolutionLbl->Caption = L"Felbontás";
		MaintasLbl->Caption = L"Képarány megtartása";
		VsyncLbl->Caption = L"VSync bekapcsolása";
		AdjmouseLbl->Caption = L"Egérérzékenység beállítás";
		DevmodeLbl->Caption = L"Kurzor bezárása az ablakba / képbe";
		RendererLbl->Caption = L"Renderelő";
		BorderLbl->Caption = L"Szegélyek megjelenítése ablakos módban";
		SavesettingsLbl->Caption = L"Ablakhelyzet és méret megjegyzése";
		ShaderLbl->Caption = L"OpenGL árnyaló";
		MaxfpsLbl->Caption = L"Képkockaszám korlátozás";
		BoxingLbl->Caption = L"Ablakos mód / felskálázás bekapcsolása";
		ToggleWindowedLbl->Caption = L"Az ablakos mód váltása";
		MaximizeWindowLbl->Caption = L"Az ablak maximalizálása";
		UnlockCursor1Lbl->Caption = L"A kurzor feloldása 1";
		UnlockCursor2Lbl->Caption = L"A kurzor feloldása 2";
		ScreenshotLbl->Caption = L"Képernyőkép";
		MaxgameticksLbl->Caption = L"Játéksebesség korlátozás";
		NoactivateappLbl->Caption = L"Alt+Tab hiba kiküszöbölése";
		ResolutionsLbl->Caption = L"További képernyőfelbontások feloldása";
		MinfpsLbl->Caption = L"Magas FPS kényszerítés / Akadozásjavítás Freesync/G-Sync esetén";
		SinglecpuLbl->Caption = L"Javítsa ki a rossz teljesítmény- és hangproblémákat";
		NonexclusiveLbl->Caption = L"Láthatatlan videók / kezelőfelületi elemek javítása";

		RendererCbx->Items->Clear();
		RendererCbx->AddItem(L"Automatikus", NULL);
		RendererCbx->AddItem(L"Direct3D 9", NULL);
		RendererCbx->AddItem(L"OpenGL", NULL);
		RendererCbx->AddItem(L"GDI", NULL);

		PresentationCbx->Items->Clear();
		PresentationCbx->AddItem(L"Teljes képernyő", NULL);
		PresentationCbx->AddItem(L"Teljes képernyő felskálázva", NULL);
		PresentationCbx->AddItem(L"Keret nélkül", NULL);
		PresentationCbx->AddItem(L"Ablakban", NULL);

		MaxgameticksCbx->Items->Clear();
		MaxgameticksCbx->AddItem(L"Korlátlan", NULL);
		MaxgameticksCbx->AddItem(L"Hangolás a kijelző képfrissítéséhez", NULL);
		MaxgameticksCbx->AddItem(L"60hz képfrissítésű kijelző emulálása", NULL);
		MaxgameticksCbx->AddItem(L"1000 tick másodpercenként", NULL);
		MaxgameticksCbx->AddItem(L"500 tick másodpercenként", NULL);
		MaxgameticksCbx->AddItem(L"250 tick másodpercenként", NULL);
		MaxgameticksCbx->AddItem(L"125 tick másodpercenként", NULL);
		MaxgameticksCbx->AddItem(L"60 tick másodpercenként", NULL);
		MaxgameticksCbx->AddItem(L"30 tick másodpercenként", NULL);
		MaxgameticksCbx->AddItem(L"25 tick másodpercenként", NULL);
		MaxgameticksCbx->AddItem(L"15 tick másodpercenként", NULL);
	}
	else if (lang == "french" || (lang == "auto" && priID == LANG_FRENCH)) {
		LanguageImg->Visible = true;

		/* -French - made by Toshiba-3 @ github */

		ConfigForm->Caption = L"Configuration cnc-ddraw";
		DisplayBtn->Caption = L"Paramètres d'Affichage";
		AdvancedBtn->Caption = L"Paramètres Avancés";
		HotkeyBtn->Caption = L"Paramètres de raccourci";
		CompatibilityBtn->Caption = L"Paramètres de Compatibilité";
		RestoreDefaultsBtn->Caption = L"Restaurer les paramètres par défaut";
		PresentationLbl->Caption = L"Présentation";
		ResolutionLbl->Caption = L"Résolution";
		MaintasLbl->Caption = L"Conserver les proportions de l'image";
		VsyncLbl->Caption = L"Activer la synchro verticale (VSync)";
		AdjmouseLbl->Caption = L"Ajuster la sensibilité souris";
		DevmodeLbl->Caption = L"Capturer le curseur dans la fenêtre / l'écran";
		RendererLbl->Caption = L"Moteur de rendu";
		BorderLbl->Caption = L"Afficher les bordures en mode fenêtré";
		SavesettingsLbl->Caption = L"Retenir position/dimensions de la fenêtre";
		ShaderLbl->Caption = L"Shader OpenGL";
		MaxfpsLbl->Caption = L"Limiter les images par seconde (FPS)";
		BoxingLbl->Caption = L"Activer windowboxing / mise à l'échelle par nombres entiers";
		ToggleWindowedLbl->Caption = L"Basculer en mode fenêtré";
		MaximizeWindowLbl->Caption = L"Agrandir la fenêtre";
		UnlockCursor1Lbl->Caption = L"Déverrouiller le curseur 1";
		UnlockCursor2Lbl->Caption = L"Déverrouiller le curseur 2";
		ScreenshotLbl->Caption = L"Capture d'écran";
		MaxgameticksLbl->Caption = L"Limiter la vitesse du jeu";
		NoactivateappLbl->Caption = L"Corriger Alt+Tab défaillant";
		ResolutionsLbl->Caption = L"Déverrouiller des résolutions d'écran supplémentaires";
		MinfpsLbl->Caption = L"Forcer FPS élevé / Corriger saccades en Freesync/G-Sync";
		SinglecpuLbl->Caption = L"Résoudre les problèmes de mauvaise performance et de son";
		NonexclusiveLbl->Caption = L"Corriger vidéos et éléments d'interface invisibles";

		RendererCbx->Items->Clear();
		RendererCbx->AddItem(L"Automatique", NULL);
		RendererCbx->AddItem(L"Direct3D 9", NULL);
		RendererCbx->AddItem(L"OpenGL", NULL);
		RendererCbx->AddItem(L"GDI", NULL);

		PresentationCbx->Items->Clear();
		PresentationCbx->AddItem(L"Plein Écran", NULL);
		PresentationCbx->AddItem(L"Plein Écran Mis à l'Échelle", NULL);
		PresentationCbx->AddItem(L"Sans Bordure", NULL);
		PresentationCbx->AddItem(L"Fenêtré", NULL);

		MaxgameticksCbx->Items->Clear();
		MaxgameticksCbx->AddItem(L"Pas de limite", NULL);
		MaxgameticksCbx->AddItem(L"Sync avec la fréquence de l'écran", NULL);
		MaxgameticksCbx->AddItem(L"Émuler un écran à 60Hz", NULL);
		MaxgameticksCbx->AddItem(L"1000 tics par seconde", NULL);
		MaxgameticksCbx->AddItem(L"500 tics par seconde", NULL);
		MaxgameticksCbx->AddItem(L"250 tics par seconde", NULL);
		MaxgameticksCbx->AddItem(L"125 tics par seconde", NULL);
		MaxgameticksCbx->AddItem(L"60 tics par seconde", NULL);
		MaxgameticksCbx->AddItem(L"30 tics par seconde", NULL);
		MaxgameticksCbx->AddItem(L"25 tics par seconde", NULL);
		MaxgameticksCbx->AddItem(L"15 tics par seconde", NULL);
	}
	else if (lang == "italian" || (lang == "auto" && priID == LANG_ITALIAN)) {
		LanguageImg->Visible = true;

		/* -Italian - made by Kappa971 @ github */

		ConfigForm->Caption = L"Configurazione di cnc-ddraw";
		DisplayBtn->Caption = L"Impostazioni dello schermo";
		AdvancedBtn->Caption = L"Impostazioni avanzate";
		HotkeyBtn->Caption = L"Tasti di scelta rapida";
		CompatibilityBtn->Caption = L"Impostazioni di compatibilità";
		RestoreDefaultsBtn->Caption = L"Ripristina le impostazioni predefinite";
		PresentationLbl->Caption = L"Presentazione";
		ResolutionLbl->Caption = L"Risoluzione";
		MaintasLbl->Caption = L"Mantieni il rapporto d'aspetto";
		VsyncLbl->Caption = L"Abilita la sincronizzazione verticale (VSync)";
		AdjmouseLbl->Caption = L"Regola la sensibilità del mouse";
		DevmodeLbl->Caption = L"Cattura il cursore nella finestra / schermo";
		RendererLbl->Caption = L"Renderer";
		BorderLbl->Caption = L"Mostra i bordi della finestra in modalità finestra";
		SavesettingsLbl->Caption = L"Ricorda la posizione e le dimensioni della finestra";
		ShaderLbl->Caption = L"Shader OpenGL";
		MaxfpsLbl->Caption = L"Limita la frequenza dei fotogrammi (FPS)";
		BoxingLbl->Caption = L"Abilita il ridimensionamento dei numeri interi";
		ToggleWindowedLbl->Caption = L"Attiva/disattiva la modalità finestra";
		MaximizeWindowLbl->Caption = L"Ingrandisci finestra";
		UnlockCursor1Lbl->Caption = L"Sblocca cursore 1";
		UnlockCursor2Lbl->Caption = L"Sblocca cursore 2";
		ScreenshotLbl->Caption = L"Istantanea dello schermo";
		MaxgameticksLbl->Caption = L"Limita la velocità di gioco";
		NoactivateappLbl->Caption = L"Correggi il funzionamento di Alt+Tab";
		ResolutionsLbl->Caption = L"Sblocca ulteriori risoluzioni dello schermo";
		MinfpsLbl->Caption = L"Forza FPS elevati / Correggi balbuzie su Freesync/G-Sync";
		SinglecpuLbl->Caption = L"Risolvi problemi di prestazioni e audio scadenti";
		NonexclusiveLbl->Caption = L"Correggi video / elementi dell'interfaccia utente invisibili";

		RendererCbx->Items->Clear();
		RendererCbx->AddItem(L"Automatico", NULL);
		RendererCbx->AddItem(L"Direct3D 9", NULL);
		RendererCbx->AddItem(L"OpenGL", NULL);
		RendererCbx->AddItem(L"GDI", NULL);

		PresentationCbx->Items->Clear();
		PresentationCbx->AddItem(L"Schermo Intero", NULL);
		PresentationCbx->AddItem(L"Schermo Intero Ridimensionato", NULL);
		PresentationCbx->AddItem(L"Senza Bordi", NULL);
		PresentationCbx->AddItem(L"In Finestra", NULL);

		MaxgameticksCbx->Items->Clear();
		MaxgameticksCbx->AddItem(L"Senza Limiti", NULL);
		MaxgameticksCbx->AddItem(L"Sincronizza con la frequenza dello schermo", NULL);
		MaxgameticksCbx->AddItem(L"Emula uno schermo a 60Hz", NULL);
		MaxgameticksCbx->AddItem(L"1000 tick al secondo", NULL);
		MaxgameticksCbx->AddItem(L"500 tick al secondo", NULL);
		MaxgameticksCbx->AddItem(L"250 tick al secondo", NULL);
		MaxgameticksCbx->AddItem(L"125 tick al secondo", NULL);
		MaxgameticksCbx->AddItem(L"60 tick al secondo", NULL);
		MaxgameticksCbx->AddItem(L"30 tick al secondo", NULL);
		MaxgameticksCbx->AddItem(L"25 tick al secondo", NULL);
		MaxgameticksCbx->AddItem(L"15 tick al secondo", NULL);
	}
	else {
		IsEnglish = true;

		try {
			if (priID == LANG_CHINESE) {
				TPngImage *png = new TPngImage();
				png->LoadFromResourceName((int)HInstance, "PngImage_CN");
				LanguageImg->Picture->Graphic = png;
				LanguageImg->Visible = true;
			}
			else if (priID == LANG_SPANISH) {
				TPngImage *png = new TPngImage();
				png->LoadFromResourceName((int)HInstance, "PngImage_ES");
				LanguageImg->Picture->Graphic = png;
				LanguageImg->Visible = true;
			}
			else if (priID == LANG_GERMAN) {
				TPngImage *png = new TPngImage();
				png->LoadFromResourceName((int)HInstance, "PngImage_DE");
				LanguageImg->Picture->Graphic = png;
				LanguageImg->Visible = true;
			}
			else if (priID == LANG_RUSSIAN) {
				TPngImage *png = new TPngImage();
				png->LoadFromResourceName((int)HInstance, "PngImage_RU");
				LanguageImg->Picture->Graphic = png;
				LanguageImg->Visible = true;
			}
			else if (priID == LANG_HUNGARIAN) {
				TPngImage *png = new TPngImage();
				png->LoadFromResourceName((int)HInstance, "PngImage_HU");
				LanguageImg->Picture->Graphic = png;
				LanguageImg->Visible = true;
			}
			else if (priID == LANG_FRENCH) {
				TPngImage *png = new TPngImage();
				png->LoadFromResourceName((int)HInstance, "PngImage_FR");
				LanguageImg->Picture->Graphic = png;
				LanguageImg->Visible = true;
			}
			else if (priID == LANG_ITALIAN) {
				TPngImage *png = new TPngImage();
				png->LoadFromResourceName((int)HInstance, "PngImage_IT");
				LanguageImg->Picture->Graphic = png;
				LanguageImg->Visible = true;
			}
		} catch (...) {
		}

		/*
		ConfigForm->Caption = L"cnc-ddraw config";
		DisplayBtn->Caption = L"Display Settings";
		AdvancedBtn->Caption = L"Advanced Settings";
		HotkeyBtn->Caption = L"Hotkey Settings";
		CompatibilityBtn->Caption = L"Compatibility Settings";
		RestoreDefaultsBtn->Caption = L"Restore default settings";
		PresentationLbl->Caption = L"Presentation";
		ResolutionLbl->Caption = L"Resolution";
		MaintasLbl->Caption = L"Maintain aspect ratio";
		VsyncLbl->Caption = L"Enable VSync";
		AdjmouseLbl->Caption = L"Adjust mouse sensitivity";
		DevmodeLbl->Caption = L"Lock cursor to window / screen";
		RendererLbl->Caption = L"Renderer";
		BorderLbl->Caption = L"Show window borders in windowed mode";
		SavesettingsLbl->Caption = L"Remember window position and size";
		ShaderLbl->Caption = L"OpenGL shader";
		MaxfpsLbl->Caption = L"Limit frame rate";
		BoxingLbl->Caption = L"Enable windowboxing / integer scaling";
		ToggleWindowedLbl->Caption = L"Toggle windowed mode";
		MaximizeWindowLbl->Caption = L"Maximize window";
		UnlockCursor1Lbl->Caption = L"Unlock cursor 1";
		UnlockCursor2Lbl->Caption = L"Unlock cursor 2";
		ScreenshotLbl->Caption = L"Screenshot";
		MaxgameticksLbl->Caption = L"Limit game speed";
		NoactivateappLbl->Caption = L"Fix broken Alt+Tab";
		ResolutionsLbl->Caption = L"Unlock additional screen resolutions";
		MinfpsLbl->Caption = L"Force high FPS / Fix stuttering on Freesync/G-Sync";
		SinglecpuLbl->Caption = L"Fix bad performance and sound issues";
		NonexclusiveLbl->Caption = L"Fix invisible videos / UI elements";

		RendererCbx->Items->Clear();
		RendererCbx->AddItem(L"Automatic", NULL);
		RendererCbx->AddItem(L"Direct3D 9", NULL);
		RendererCbx->AddItem(L"OpenGL", NULL);
		RendererCbx->AddItem(L"GDI", NULL);

		PresentationCbx->Items->Clear();
		PresentationCbx->AddItem(L"Fullscreen", NULL);
		PresentationCbx->AddItem(L"Fullscreen Upscaled", NULL);
		PresentationCbx->AddItem(L"Borderless", NULL);
		PresentationCbx->AddItem(L"Windowed", NULL);

		MaxgameticksCbx->Items->Clear();
		MaxgameticksCbx->AddItem(L"No limit", NULL);
		MaxgameticksCbx->AddItem(L"Sync with monitor refresh rate", NULL);
		MaxgameticksCbx->AddItem(L"Emulate 60hz refresh rate monitor", NULL);
		MaxgameticksCbx->AddItem(L"1000 ticks per second", NULL);
		MaxgameticksCbx->AddItem(L"500 ticks per second", NULL);
		MaxgameticksCbx->AddItem(L"250 ticks per second", NULL);
		MaxgameticksCbx->AddItem(L"125 ticks per second", NULL);
		MaxgameticksCbx->AddItem(L"60 ticks per second", NULL);
		MaxgameticksCbx->AddItem(L"30 ticks per second", NULL);
		MaxgameticksCbx->AddItem(L"25 ticks per second", NULL);
		MaxgameticksCbx->AddItem(L"15 ticks per second", NULL);
		*/
	}

	ToggleWindowedKeyLbl->Caption = GetKeyText(VK_MENU) + L" +";
	MaximizeWindowKeyLbl->Caption = GetKeyText(VK_MENU) + L" +";
	UnlockCursor1KeyLbl->Caption = GetKeyText(VK_CONTROL) + L" +";
	UnlockCursor2KeyLbl->Caption = "R " + GetKeyText(VK_MENU) + L" +";

	ConfigForm->Caption +=
		" (" + TPath::GetFileName(
					TPath::GetDirectoryName(Application->ExeName)) + ")";
}

void __fastcall TConfigForm::DisplayBtnClick(TObject *Sender)
{
	DisplayPnl->Visible = true;
	AdvancedPnl->Visible = false;
	HotkeyPnl->Visible = false;
	CompatibilityPnl->Visible = false;
}

void __fastcall TConfigForm::AdvancedBtnClick(TObject *Sender)
{
	AdvancedPnl->Visible = true;
	DisplayPnl->Visible = false;
	HotkeyPnl->Visible = false;
	CompatibilityPnl->Visible = false;
}

void __fastcall TConfigForm::HotkeyBtnClick(TObject *Sender)
{
	HotkeyPnl->Visible = true;
	AdvancedPnl->Visible = false;
	DisplayPnl->Visible = false;
	CompatibilityPnl->Visible = false;
}

void __fastcall TConfigForm::CompatibilityBtnClick(TObject *Sender)
{
	CompatibilityPnl->Visible = true;
	AdvancedPnl->Visible = false;
	DisplayPnl->Visible = false;
	HotkeyPnl->Visible = false;
}

void __fastcall TConfigForm::FormCreate(TObject *Sender)
{
	/* Let cnc-ddraw create a new ddraw.ini if it doesn't exist */
	if (FileExists(GAME_PATH + "ddraw.dll") && !FileExists(GAME_PATH + "ddraw.ini")) {

		SetEnvironmentVariableW(L"cnc_ddraw_config_init", L"1");

		HMODULE ddraw = LoadLibraryW((GAME_PATH + "ddraw.dll").w_str());

		if (ddraw) {

			void (WINAPI* dd_create)(void*, void**, void*);

			dd_create =
				(void (WINAPI*)(void*, void**, void*))
					GetProcAddress(ddraw, "DirectDrawCreate");

			if (dd_create && GetProcAddress(ddraw, "GameHandlesClose")) {

				void *buf;
				dd_create(NULL, &buf, NULL);
			}

			FreeLibrary(ddraw);
		}
	}

	auto *ini = new TIniFile(GAME_PATH + "ddraw.ini");
	auto *hd_ini = new TIniFile(GAME_PATH + "Warcraft II BNE.ini");

	if (ini->ReadString("ddraw", "configtheme", "Windows10") == "Cobalt XEMedia") {

		ThemePnl->Color = (TColor)RGB(243, 243, 243);
		DisplayPnl->StyleElements = TStyleElements(seFont + seClient + seBorder);
		AdvancedPnl->StyleElements = TStyleElements(seFont + seClient + seBorder);
		HotkeyPnl->StyleElements = TStyleElements(seFont + seClient + seBorder);
		CompatibilityPnl->StyleElements = TStyleElements(seFont + seClient + seBorder);

		MenuPnl->StyleElements = TStyleElements(seFont);
		MenuPnl->Color = (TColor)RGB(31, 31, 31);
	}

	ApplyTranslation(ini);

	/* Display Settings */

	bool windowed = GetBool(ini, "windowed", false);
	bool fullscreen = GetBool(ini, "fullscreen", false);

	if (windowed && fullscreen) {
		PresentationCbx->ItemIndex = 2;
	}
	else if (windowed) {
		PresentationCbx->ItemIndex = 3;
	}
	else if (fullscreen) {
		PresentationCbx->ItemIndex = 1;
	}
	else {
		PresentationCbx->ItemIndex = 0;
	}

	int cwidth = hd_ini->ReadInteger("Game", "Width", 0);
	int cheight = hd_ini->ReadInteger("Game", "Height", 0);
	int reso = hd_ini->ReadInteger("Game", "Resolution", 4);

	if (!cwidth || !cheight) {
		ResolutionCbx->ItemIndex = reso <= 11 ? reso : 4;
	}
	else {
		ResolutionCbx->AddItem(IntToStr(cwidth) + "x" + IntToStr(cheight), NULL);
		ResolutionCbx->ItemIndex = ResolutionCbx->Items->Count - 1;
	}

	MaintasChk->State = GetBool(ini, "maintas", false) ? tssOn : tssOff;
	VsyncChk->State = GetBool(ini, "vsync", false) ? tssOn : tssOff;
	AdjmouseChk->State = GetBool(ini, "adjmouse", true) ? tssOn : tssOff;
	DevmodeChk->State = GetBool(ini, "devmode", false) ? tssOff : tssOn;

	/* Advanced Settings */

	auto renderer = LowerCase(ini->ReadString("ddraw", "renderer", "auto"));

	if (renderer == "direct3d9on12") {
		RendererCbx->AddItem(L"Direct3D 12 (9On12)", NULL);
		RendererCbx->ItemIndex = 4;

		ShaderLbl->Caption =
			ReplaceStr(ShaderLbl->Caption, "OpenGL", "Direct3D");

		ShaderD3DCbx->Visible = true;
		ShaderCbx->Visible = false;
	}
	else if (renderer == "openglcore") {
		RendererCbx->AddItem(L"OpenGL Core", NULL);
		RendererCbx->ItemIndex = 4;
	}
	else if (StartsStr("d", renderer)) {
		RendererCbx->ItemIndex = 1;

		ShaderLbl->Caption =
			ReplaceStr(ShaderLbl->Caption, "OpenGL", "Direct3D");

		ShaderD3DCbx->Visible = true;
		ShaderCbx->Visible = false;
	}
	else if (StartsStr("o", renderer)) {
		RendererCbx->ItemIndex = 2;
	}
	else if (StartsStr("s", renderer) || StartsStr("g", renderer)) {
		RendererCbx->ItemIndex = 3;
	}
	else {
		RendererCbx->ItemIndex = 0;
	}

	try
	{
		TStringDynArray list = TDirectory::GetFiles(
			"Shaders",
			"*.glsl",
			TSearchOption::soAllDirectories);

		for (int i = 0; i < list.Length; i++)
			ShaderCbx->AddItem(list[i], NULL);
	}
	catch (...)
	{
	}	

	auto shader = ini->ReadString("ddraw", "shader", "Bicubic");
	ShaderCbx->ItemIndex = ShaderCbx->Items->IndexOf(shader);

	if (ShaderCbx->ItemIndex == -1) {
		ShaderCbx->AddItem(shader, NULL);
		ShaderCbx->ItemIndex = ShaderCbx->Items->Count - 1;
	}

	bool d3d9_filter = GetBool(ini, "d3d9linear", true);

	if (d3d9_filter) {
		ShaderD3DCbx->ItemIndex = 1;
	}
	else {
		ShaderD3DCbx->ItemIndex = 0;
	}
	
	Maxfps = ini->ReadInteger("ddraw", "maxfps", -1);
	MaxfpsChk->State = Maxfps != 0 ? tssOn : tssOff;

	BoxingChk->State = GetBool(ini, "boxing", false) ? tssOn : tssOff;
	BorderChk->State = GetBool(ini, "border", true) ? tssOn : tssOff;

	Savesettings = ini->ReadInteger("ddraw", "savesettings", 1);
	SavesettingsChk->State = Savesettings != 0 ? tssOn : tssOff;

	/* Hotkey Settings */

	ToggleWindowedEdt->Text =
		GetKeyText(Byte(ini->ReadInteger("ddraw", "keytogglefullscreen", 0x0D)));

	MaximizeWindowEdt->Text =
		GetKeyText(Byte(ini->ReadInteger("ddraw", "keytogglemaximize", 0x22)));

	UnlockCursor1Edt->Text =
		GetKeyText(Byte(ini->ReadInteger("ddraw", "keyunlockcursor1", 0x09)));

	UnlockCursor2Edt->Text =
		GetKeyText(Byte(ini->ReadInteger("ddraw", "keyunlockcursor2", 0xA3)));

	ScreenshotEdt->Text =
		GetKeyText(Byte(ini->ReadInteger("ddraw", "keyscreenshot", 0x2C)));

	/* Compatibility Settings */

	int maxgameticks = ini->ReadInteger("ddraw", "maxgameticks", 0);

	switch (maxgameticks) {
	case -1:
		MaxgameticksCbx->ItemIndex = 0;
		break;
	case -2:
		MaxgameticksCbx->ItemIndex = 1;
		break;
	case 0:
		MaxgameticksCbx->ItemIndex = 2;
		break;
	case 1000:
		MaxgameticksCbx->ItemIndex = 3;
		break;
	case 500:
		MaxgameticksCbx->ItemIndex = 4;
		break;
	case 250:
		MaxgameticksCbx->ItemIndex = 5;
		break;
	case 125:
		MaxgameticksCbx->ItemIndex = 6;
		break;
	case 60:
		MaxgameticksCbx->ItemIndex = 7;
		break;
	case 30:
		MaxgameticksCbx->ItemIndex = 8;
		break;
	case 25:
		MaxgameticksCbx->ItemIndex = 9;
		break;
	case 15:
		MaxgameticksCbx->ItemIndex = 10;
		break;
	default:
		MaxgameticksCbx->AddItem(IntToStr(maxgameticks), NULL);
		MaxgameticksCbx->ItemIndex = 11;
		break;
	}

	NoactivateappChk->State = GetBool(ini, "noactivateapp", false) ? tssOn : tssOff;

	Resolutions = ini->ReadInteger("ddraw", "resolutions", 0);
	ResolutionsChk->State = Resolutions == 2 ? tssOn : tssOff;

	Minfps = ini->ReadInteger("ddraw", "minfps", 0);
	MinfpsChk->State = Minfps != 0 ? tssOn : tssOff;

	SinglecpuChk->State = GetBool(ini, "singlecpu", true) ? tssOff : tssOn;
	NonexclusiveChk->State = GetBool(ini, "nonexclusive", false) ? tssOn : tssOff;

	CompatibilityBtn->Visible = !GetBool(ini, "hide_compat_tab", false);

	RestoreDefaultsBtn->Visible =
		FileExists(GAME_PATH + "ddraw.dll") &&
		FileExists(GAME_PATH + "ddraw.ini") &&
		GetBool(ini, "allow_reset", true);

	delete ini;
	delete hd_ini;

	Initialized = true;
}

void TConfigForm::SaveSettings()
{
	if (!Initialized)
		return;

	auto *ini = new TIniFile(GAME_PATH + "ddraw.ini");
	auto *hd_ini = new TIniFile(GAME_PATH + "Warcraft II BNE.ini");

	/* Display Settings */

	switch(PresentationCbx->ItemIndex) {
	case 0:
		ini->WriteString("ddraw", "windowed", "false");
		ini->WriteString("ddraw", "fullscreen", "false");
		ini->WriteString("ddraw", "toggle_borderless", "false");
		ini->WriteString("ddraw", "toggle_upscaled", "false");
		break;
	case 1:
		ini->WriteString("ddraw", "windowed", "false");
		ini->WriteString("ddraw", "fullscreen", "true");
		ini->WriteString("ddraw", "toggle_borderless", "false");
		ini->WriteString("ddraw", "toggle_upscaled", "true");
		break;
	case 2:
		ini->WriteString("ddraw", "windowed", "true");
		ini->WriteString("ddraw", "fullscreen", "true");
		ini->WriteString("ddraw", "toggle_borderless", "true");
		ini->WriteString("ddraw", "toggle_upscaled", "false");
		break;
	case 3:
		ini->WriteString("ddraw", "windowed", "true");
		ini->WriteString("ddraw", "fullscreen", "false");
		break;
	default:
		break;
	}

	if (ResolutionCbx->ItemIndex <= 11) {
		hd_ini->WriteInteger("Game", "Resolution", ResolutionCbx->ItemIndex);

		hd_ini->WriteInteger("Game", "Width", 0);
		hd_ini->WriteInteger("Game", "Height", 0);

		if (ResolutionCbx->Items->Count == 13) {
			ResolutionCbx->Items->Delete(12);
		}			
	}

	ini->WriteString(
		"ddraw",
		"maintas",
		MaintasChk->State == tssOn ? "true" : "false");

	ini->WriteString(
		"ddraw",
		"vsync",
		VsyncChk->State == tssOn ? "true" : "false");

	ini->WriteString(
		"ddraw",
		"adjmouse",
		AdjmouseChk->State == tssOn ? "true" : "false");

	ini->WriteString(
		"ddraw",
		"devmode",
		DevmodeChk->State == tssOn ? "false" : "true");

	/* Advanced Settings */

	switch(RendererCbx->ItemIndex) {
	case 0:
		ini->WriteString("ddraw", "renderer", "auto");
		break;
	case 1:
		ini->WriteString("ddraw", "renderer", "direct3d9");
		break;
	case 2:
		ini->WriteString("ddraw", "renderer", "opengl");
		break;
	case 3:
		ini->WriteString("ddraw", "renderer", "gdi");
		break;
	case 4:
		if (RendererCbx->Text == "OpenGL Core") {
			ini->WriteString("ddraw", "renderer", "openglcore");
		}
		else {
			ini->WriteString("ddraw", "renderer", "direct3d9on12");
		}
		break;
	default:
		break;
	}

	ini->WriteString("ddraw", "shader", ShaderCbx->Text);

	if (ShaderD3DCbx->ItemIndex == 0) {
		ini->WriteString("ddraw", "d3d9linear", "false");
	}
	else {
		ini->WriteString("ddraw", "d3d9linear", "true");
	}
	
	int maxfps = Maxfps == 0 ? -1 : Maxfps;

	ini->WriteInteger(
		"ddraw",
		"maxfps",
		MaxfpsChk->State == tssOn ? maxfps : 0);

	ini->WriteString(
		"ddraw",
		"boxing",
		BoxingChk->State == tssOn ? "true" : "false");

	ini->WriteString(
		"ddraw",
		"border",
		BorderChk->State == tssOn ? "true" : "false");

	int savesettings = Savesettings == 0 ? 1 : Savesettings;

	ini->WriteInteger(
		"ddraw",
		"savesettings",
		SavesettingsChk->State == tssOn ? savesettings : 0);

	if (Savesettings != 0 && SavesettingsChk->State == tssOff) {
		ini->WriteInteger("ddraw", "width", 0);
		ini->WriteInteger("ddraw", "height", 0);
		ini->WriteInteger("ddraw", "posX", -32000);
		ini->WriteInteger("ddraw", "posY", -32000);
	}

	/* Hotkey Settings */

	ini->WriteString(
		"ddraw",
		"keytogglefullscreen",
		"0x" + IntToHex(Byte(GetKeyCode(ToggleWindowedEdt->Text))));

	ini->WriteString(
		"ddraw",
		"keytogglemaximize",
		"0x" + IntToHex(Byte(GetKeyCode(MaximizeWindowEdt->Text))));

	ini->WriteString(
		"ddraw",
		"keyunlockcursor1",
		"0x" + IntToHex(Byte(GetKeyCode(UnlockCursor1Edt->Text))));

	ini->WriteString(
		"ddraw",
		"keyunlockcursor2",
		"0x" + IntToHex(Byte(GetKeyCode(UnlockCursor2Edt->Text))));

	ini->WriteString(
		"ddraw",
		"keyscreenshot",
		"0x" + IntToHex(Byte(GetKeyCode(ScreenshotEdt->Text))));

	/* Compatibility Settings */

	switch(MaxgameticksCbx->ItemIndex) {
	case 0:
		ini->WriteInteger("ddraw", "maxgameticks", -1);
		break;
	case 1:
		ini->WriteInteger("ddraw", "maxgameticks", -2);
		break;
	case 2:
		ini->WriteInteger("ddraw", "maxgameticks", 0);
		break;
	case 3:
		ini->WriteInteger("ddraw", "maxgameticks", 1000);
		break;
	case 4:
		ini->WriteInteger("ddraw", "maxgameticks", 500);
		break;
	case 5:
		ini->WriteInteger("ddraw", "maxgameticks", 250);
		break;
	case 6:
		ini->WriteInteger("ddraw", "maxgameticks", 125);
		break;
	case 7:
		ini->WriteInteger("ddraw", "maxgameticks", 60);
		break;
	case 8:
		ini->WriteInteger("ddraw", "maxgameticks", 30);
		break;
	case 9:
		ini->WriteInteger("ddraw", "maxgameticks", 25);
		break;
	case 10:
		ini->WriteInteger("ddraw", "maxgameticks", 15);
		break;
	case 11:
		ini->WriteString("ddraw", "maxgameticks", MaxgameticksCbx->Text);
		break;
	default:
		break;
	}

	ini->WriteString(
		"ddraw",
		"noactivateapp",
		NoactivateappChk->State == tssOn ? "true" : "false");

	int resolutions = Resolutions != 2 ? Resolutions : 0;

	ini->WriteInteger(
		"ddraw",
		"resolutions",
		ResolutionsChk->State == tssOn ? 2 : resolutions);

	int minfps = Minfps == 0 ? -1 : Minfps;

	ini->WriteInteger(
		"ddraw",
		"minfps",
		MinfpsChk->State == tssOn ? minfps : 0);

	ini->WriteString(
		"ddraw",
		"singlecpu",
		SinglecpuChk->State == tssOn ? "false" : "true");

	ini->WriteString(
		"ddraw",
		"nonexclusive",
		NonexclusiveChk->State == tssOn ? "true" : "false");

	delete ini;
	delete hd_ini;
}

void __fastcall TConfigForm::FormActivate(TObject *Sender)
{
	/* Detect wine (Linux/macOS) and create the needed dll override */
	if (!GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "wine_get_version"))
		return;

	TRegistry* reg = new TRegistry(KEY_READ);
	reg->RootKey = HKEY_CURRENT_USER;

	if (reg->OpenKey("Software\\Wine\\DllOverrides\\", true)) {

		if (!reg->ValueExists("ddraw")) {

			reg->CloseKey();

			if (Application->MessageBox(
				L"cnc-ddraw requires a dll override in winecfg, "
					"would you like to add it now?",
				L"cnc-ddraw",
				MB_YESNO) == IDNO) {

				reg->Free();
				return;
			}

			reg->Access = KEY_WRITE;

			if (reg->OpenKey("Software\\Wine\\DllOverrides\\", true)) {

				reg->WriteString("ddraw", "native,builtin");
				reg->CloseKey();
			}
		}
		else
			reg->CloseKey();
	}

	reg->Free();
}

void __fastcall TConfigForm::HotkeyEdtKeyDown(TObject *Sender, WORD &Key, TShiftState Shift)
{
	TEdit *edit = static_cast<TEdit*>(Sender);

	if (Key == VK_DELETE || Key == VK_BACK) {
		edit->Text = L"";
	}
	else if (GetAsyncKeyState(VK_RCONTROL) & 0x8000) {
		edit->Text = GetKeyText(VK_RCONTROL);
	}
	else {
		edit->Text = GetKeyText(Key);
	}
}

void __fastcall TConfigForm::HotkeyEdtKeyUp(TObject *Sender, WORD &Key, TShiftState Shift)
{
	TEdit *edit = static_cast<TEdit*>(Sender);

	if (Key == VK_SNAPSHOT || Key == VK_TAB) {
		edit->Text = GetKeyText(Key);
	}

	SaveSettings();
}

WORD TConfigForm::GetKeyCode(System::UnicodeString text)
{
	if (text == L"PrtScn") {
		return VK_SNAPSHOT;
	}

	if (text == L"Pause_") {
		return VK_PAUSE;
	}

	if (text == L"R " + KeyToText(VK_CONTROL)) {
		return VK_RCONTROL;
	}

	for (WORD i = 0; i < 256; i++) {
		if (KeyNames[i] == text) {
			return i;
		}
	}

	return 0;
}

System::UnicodeString TConfigForm::GetKeyText(WORD key)
{
	if (key == VK_SNAPSHOT) {
		return L"PrtScn";
	}

	if (key == VK_PAUSE) {
		return L"Pause_";
	}

	if (key == VK_RCONTROL) {
		return L"R " + KeyToText(VK_CONTROL);
	}

	if (key == VK_RSHIFT) {
		return KeyToText(VK_SHIFT);
	}

	return KeyToText(key);
}

System::UnicodeString TConfigForm::KeyToText(WORD key)
{
	UINT scanCode = MapVirtualKeyW(key, MAPVK_VK_TO_VSC);

	switch (key) {
		case VK_LEFT:
		case VK_UP:
		case VK_RIGHT:
		case VK_DOWN:
		case VK_PRIOR:
		case VK_NEXT:
		case VK_END:
		case VK_HOME:
		case VK_INSERT:
		case VK_DELETE:
		case VK_DIVIDE:
		case VK_NUMLOCK:
		{
			scanCode |= 0x100;
			break;
		}
	}

	WCHAR keyName[256] = {};
	GetKeyNameTextW(scanCode << 16, keyName, sizeof(keyName) / sizeof(WCHAR));

	KeyNames[(BYTE)key] = keyName;

	return KeyNames[(BYTE)key];
}

bool TConfigForm::GetBool(TIniFile *ini, System::UnicodeString key, bool defValue)
{
	auto s = LowerCase(ini->ReadString("ddraw", key, defValue ? "true" : "false"));
	return s == "true" || s == "yes" || s == "1";
}

void __fastcall TConfigForm::PresentationCbxChange(TObject *Sender)
{
	SaveSettings();
}

void __fastcall TConfigForm::MaintasChkClick(TObject *Sender)
{
	SaveSettings();
}

void __fastcall TConfigForm::VsyncChkClick(TObject *Sender)
{
	SaveSettings();
}

void __fastcall TConfigForm::AdjmouseChkClick(TObject *Sender)
{
	SaveSettings();
}

void __fastcall TConfigForm::DevmodeChkClick(TObject *Sender)
{
	SaveSettings();
}

void __fastcall TConfigForm::RendererCbxChange(TObject *Sender)
{
	if (ContainsStr(RendererCbx->Text, "Direct3D")) {

		ShaderLbl->Caption =
			ReplaceStr(ShaderLbl->Caption, "OpenGL", "Direct3D");

		ShaderD3DCbx->Visible = true;
		ShaderCbx->Visible = false;
	}
	else {
		ShaderLbl->Caption =
			ReplaceStr(ShaderLbl->Caption, "Direct3D", "OpenGL");

		ShaderCbx->Visible = true;
		ShaderD3DCbx->Visible = false;
	}

	SaveSettings();
}

void __fastcall TConfigForm::ShaderCbxChange(TObject *Sender)
{
	if (RendererCbx->Text != "OpenGL Core") {
		RendererCbx->ItemIndex = 2;
	}

	SaveSettings();
}

void __fastcall TConfigForm::ShaderD3DCbxChange(TObject *Sender)
{
	SaveSettings();
}

void __fastcall TConfigForm::MaxfpsChkClick(TObject *Sender)
{
	SaveSettings();
}

void __fastcall TConfigForm::BoxingChkClick(TObject *Sender)
{
	SaveSettings();
}

void __fastcall TConfigForm::BorderChkClick(TObject *Sender)
{
	SaveSettings();
}

void __fastcall TConfigForm::SavesettingsChkClick(TObject *Sender)
{
	SaveSettings();
}

void __fastcall TConfigForm::MaxgameticksCbxChange(TObject *Sender)
{
	SaveSettings();
}

void __fastcall TConfigForm::NoactivateappChkClick(TObject *Sender)
{
	SaveSettings();
}

void __fastcall TConfigForm::ResolutionsChkClick(TObject *Sender)
{
	SaveSettings();
}

void __fastcall TConfigForm::MinfpsChkClick(TObject *Sender)
{
	SaveSettings();
}

void __fastcall TConfigForm::SinglecpuChkClick(TObject *Sender)
{
	SaveSettings();
}

void __fastcall TConfigForm::NonexclusiveChkClick(TObject *Sender)
{
	SaveSettings();
}

void __fastcall TConfigForm::PboxPaint(TObject *Sender)
{
	TPaintBox *pbox = static_cast<TPaintBox*>(Sender);
	//pbox->Canvas->Rectangle(pbox->ClientRect);
}


