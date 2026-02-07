;InnoSetupVersion=6.1.0 (Unicode)

[Setup]
AppName=Spotify ADS Remover
AppId=Spotify ADS Remover
AppVersion=v1.2.59.510
AppPublisher=Spotify
DefaultDirName={userappdata}\Spotify
DefaultGroupName=Spotify
OutputBaseFilename=Setup
Compression=lzma2
DisableDirPage=yes
DisableProgramGroupPage=auto
WizardImageFile=embedded\WizardImage0.bmp,embedded\WizardImage1.bmp
WizardSmallImageFile=embedded\WizardSmallImage0.bmp,embedded\WizardSmallImage1.bmp,embedded\WizardSmallImage2.bmp,embedded\WizardSmallImage3.bmp,embedded\WizardSmallImage4.bmp,embedded\WizardSmallImage5.bmp,embedded\WizardSmallImage6.bmp

[Files]
Source: "{app}\chrome_100_percent.pak"; DestDir: "{app}"; Flags: ignoreversion 
Source: "{app}\chrome_200_percent.pak"; DestDir: "{app}"; Flags: ignoreversion 
Source: "{app}\chrome_elf.dll"; DestDir: "{app}"; Flags: ignoreversion 
Source: "{app}\crash_reporter.cfg"; DestDir: "{app}"; Flags: ignoreversion 
Source: "{app}\d3dcompiler_47.dll"; DestDir: "{app}"; Flags: ignoreversion 
Source: "{app}\icudtl.dat"; DestDir: "{app}"; Flags: ignoreversion 
Source: "{app}\libcef.dll"; DestDir: "{app}"; Flags: ignoreversion 
Source: "{app}\libcef.dll.sig"; DestDir: "{app}"; Flags: ignoreversion 
Source: "{app}\libEGL.dll"; DestDir: "{app}"; Flags: ignoreversion 
Source: "{app}\libGLESv2.dll"; DestDir: "{app}"; Flags: ignoreversion 
Source: "{app}\prefs"; DestDir: "{app}"; Flags: ignoreversion 
Source: "{app}\resources.pak"; DestDir: "{app}"; Flags: ignoreversion 
Source: "{app}\Spotify.exe"; DestDir: "{app}"; Flags: ignoreversion 
Source: "{app}\Spotify.exe.sig"; DestDir: "{app}"; Flags: ignoreversion 
Source: "{app}\v8_context_snapshot.bin"; DestDir: "{app}"; Flags: ignoreversion 
Source: "{app}\vk_swiftshader.dll"; DestDir: "{app}"; Flags: ignoreversion 
Source: "{app}\vk_swiftshader_icd.json"; DestDir: "{app}"; Flags: ignoreversion 
Source: "{app}\vulkan-1.dll"; DestDir: "{app}"; Flags: ignoreversion 
Source: "{app}\Apps\login.spa"; DestDir: "{app}\Apps"; Flags: ignoreversion 
Source: "{app}\Apps\xpui.bak"; DestDir: "{app}\Apps"; Flags: ignoreversion 
Source: "{app}\Apps\xpui.spa"; DestDir: "{app}\Apps"; Flags: ignoreversion 
Source: "{app}\locales\af.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\am.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\ar-EG.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\ar-MA.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\ar-SA.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\ar.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\az.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\bg.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\bho.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\bn.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\bs.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\ca.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\cs.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\da.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\de.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\el.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\en-GB.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\en-US.pak"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\en.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\es-419.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\es-AR.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\es-MX.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\es.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\et.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\eu.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\fa.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\fi.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\fil.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\fr-CA.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\fr.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\gl.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\gu.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\he.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\hi.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\hr.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\hu.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\id.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\is.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\it.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\ja.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\kn.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\ko.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\lt.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\lv.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\mk.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\ml.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\mr.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\ms.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\nb.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\ne.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\nl.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\or.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\pa-IN.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\pa-PK.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\pl.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\pt-BR.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\pt-PT.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\ro.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\ru.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\sk.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\sl.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\sr.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\sv.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\sw.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\ta.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\te.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\th.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\tr.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\uk.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\ur.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\vi.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\zh-CN.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\zh-Hant-HK.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\zh-Hant.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\zh-TW.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 
Source: "{app}\locales\zu.mo"; DestDir: "{app}\locales"; Flags: ignoreversion 

[Icons]
Name: "{group}\Spotify"; Filename: "{app}\Spotify.exe"; MinVersion: 0.0,5.0; 
Name: "{commondesktop}\Spotify"; Filename: "{app}\Spotify.exe"; Tasks: desktopicon; MinVersion: 0.0,5.0; 

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; MinVersion: 0.0,5.0; 
Name: "quicklaunchicon"; Description: "{cm:CreateQuickLaunchIcon}"; GroupDescription: "{cm:AdditionalIcons}"; MinVersion: 0.0,5.0; OnlyBelowVersion: 0.0,6.01; 

[CustomMessages]
english.NameAndVersion=%1 version %2
english.AdditionalIcons=Additional shortcuts:
english.CreateDesktopIcon=Create a &desktop shortcut
english.CreateQuickLaunchIcon=Create a &Quick Launch shortcut
english.ProgramOnTheWeb=%1 on the Web
english.UninstallProgram=Uninstall %1
english.LaunchProgram=Launch %1
english.AssocFileExtension=&Associate %1 with the %2 file extension
english.AssocingFileExtension=Associating %1 with the %2 file extension...
english.AutoStartProgramGroupDescription=Startup:
english.AutoStartProgram=Automatically start %1
english.AddonHostProgramNotFound=%1 could not be located in the folder you selected.%n%nDo you want to continue anyway?
brazilianportuguese.NameAndVersion=%1 versão %2
brazilianportuguese.AdditionalIcons=Atalhos adicionais:
brazilianportuguese.CreateDesktopIcon=Criar um atalho &na área de trabalho
brazilianportuguese.CreateQuickLaunchIcon=Criar um atalho na &barra de inicialização rápida
brazilianportuguese.ProgramOnTheWeb=%1 na Web
brazilianportuguese.UninstallProgram=Desinstalar o %1
brazilianportuguese.LaunchProgram=Iniciar o %1
brazilianportuguese.AssocFileExtension=&Associar o %1 com a extensão do arquivo %2
brazilianportuguese.AssocingFileExtension=Associando o %1 com a extensão do arquivo %2...
brazilianportuguese.AutoStartProgramGroupDescription=Inicialização:
brazilianportuguese.AutoStartProgram=Iniciar o %1 automaticamente
brazilianportuguese.AddonHostProgramNotFound=O %1 não pôde ser localizado na pasta que você selecionou.%n%nVocê quer continuar de qualquer maneira?
catalan.NameAndVersion=%1 versió %2
catalan.AdditionalIcons=Icones addicionals:
catalan.CreateDesktopIcon=Crea una icona a l'&Escriptori
catalan.CreateQuickLaunchIcon=Crea una icona a la &Barra de tasques
catalan.ProgramOnTheWeb=%1 a Internet
catalan.UninstallProgram=Desinstal·la %1
catalan.LaunchProgram=Obre %1
catalan.AssocFileExtension=&Associa %1 amb l'extensió de fitxer %2
catalan.AssocingFileExtension=Associant %1 amb l'extensió de fitxer %2...
catalan.AutoStartProgramGroupDescription=Inici:
catalan.AutoStartProgram=Inicia automàticament %1
catalan.AddonHostProgramNotFound=%1 no ha pogut ser trobat a la carpeta seleccionada.%n%nVoleu continuar igualment?
corsican.NameAndVersion=%1 versione %2
corsican.AdditionalIcons=Accurtatoghji addizziunali :
corsican.CreateDesktopIcon=Creà un accurtatoghju nant’à u &scagnu
corsican.CreateQuickLaunchIcon=Creà un accurtatoghju nant’à a barra di &lanciu prontu
corsican.ProgramOnTheWeb=%1 nant’à u Web
corsican.UninstallProgram=Disinstallà %1
corsican.LaunchProgram=Lancià %1
corsican.AssocFileExtension=&Assucià %1 cù l’estensione di schedariu %2
corsican.AssocingFileExtension=Associu di %1 cù l’estensione di schedariu %2…
corsican.AutoStartProgramGroupDescription=Lanciu autumaticu :
corsican.AutoStartProgram=Lanciu autumaticu di %1
corsican.AddonHostProgramNotFound=Impussibule di truvà %1 in u cartulare selezziunatu.%n%nVulete cuntinuà l’installazione quantunque ?
czech.NameAndVersion=%1 verze %2
czech.AdditionalIcons=Další zástupci:
czech.CreateDesktopIcon=Vytvořit zástupce na &ploše
czech.CreateQuickLaunchIcon=Vytvořit zástupce na panelu &Snadné spuštění
czech.ProgramOnTheWeb=Aplikace %1 na internetu
czech.UninstallProgram=Odinstalovat aplikaci %1
czech.LaunchProgram=Spustit aplikaci %1
czech.AssocFileExtension=Vytvořit &asociaci mezi soubory typu %2 a aplikací %1
czech.AssocingFileExtension=Vytváří se asociace mezi soubory typu %2 a aplikací %1...
czech.AutoStartProgramGroupDescription=Po spuštění:
czech.AutoStartProgram=Spouštět aplikaci %1 automaticky
czech.AddonHostProgramNotFound=Aplikace %1 nebyla ve Vámi zvolené složce nalezena.%n%nChcete přesto pokračovat?
danish.NameAndVersion=%1 version %2
danish.AdditionalIcons=Supplerende ikoner:
danish.CreateDesktopIcon=Opret ikon på skrive&bordet
danish.CreateQuickLaunchIcon=Opret &hurtigstart-ikon
danish.ProgramOnTheWeb=%1 på internettet
danish.UninstallProgram=Afinstaller (fjern) %1
danish.LaunchProgram=&Start %1
danish.AssocFileExtension=Sammen&kæd %1 med filtypen %2
danish.AssocingFileExtension=Sammenkæder %1 med filtypen %2...
danish.AutoStartProgramGroupDescription=Start:
danish.AutoStartProgram=Start automatisk %1
danish.AddonHostProgramNotFound=%1 blev ikke fundet i den valgte mappe.%n%nVil du alligevel fortsætte?
dutch.NameAndVersion=%1 versie %2
dutch.AdditionalIcons=Extra snelkoppelingen:
dutch.CreateDesktopIcon=Maak een snelkoppeling op het &bureaublad
dutch.CreateQuickLaunchIcon=Maak een snelkoppeling op de &Snel starten werkbalk
dutch.ProgramOnTheWeb=%1 op het Web
dutch.UninstallProgram=Verwijder %1
dutch.LaunchProgram=&Start %1
dutch.AssocFileExtension=&Koppel %1 aan de %2 bestandsextensie
dutch.AssocingFileExtension=Bezig met koppelen van %1 aan de %2 bestandsextensie...
dutch.AutoStartProgramGroupDescription=Opstarten:
dutch.AutoStartProgram=%1 automatisch starten
dutch.AddonHostProgramNotFound=%1 kon niet worden gevonden in de geselecteerde map.%n%nWilt u toch doorgaan?
finnish.NameAndVersion=%1 versio %2
finnish.AdditionalIcons=Lisäkuvakkeet:
finnish.CreateDesktopIcon=Lu&o kuvake työpöydälle
finnish.CreateQuickLaunchIcon=Luo kuvake &pikakäynnistyspalkkiin
finnish.ProgramOnTheWeb=%1 Internetissä
finnish.UninstallProgram=Poista %1
finnish.LaunchProgram=&Käynnistä %1
finnish.AssocFileExtension=&Yhdistä %1 tiedostopäätteeseen %2
finnish.AssocingFileExtension=Yhdistetään %1 tiedostopäätteeseen %2 ...
finnish.AutoStartProgramGroupDescription=Käynnistys:
finnish.AutoStartProgram=Käynnistä %1 automaattisesti
finnish.AddonHostProgramNotFound=%1 ei ole valitsemassasi kansiossa.%n%nHaluatko jatkaa tästä huolimatta?
french.NameAndVersion=%1 version %2
french.AdditionalIcons=Icônes supplémentaires :
french.CreateDesktopIcon=Créer une icône sur le &Bureau
french.CreateQuickLaunchIcon=Créer une icône dans la barre de &Lancement rapide
french.ProgramOnTheWeb=Page d'accueil de %1
french.UninstallProgram=Désinstaller %1
french.LaunchProgram=Exécuter %1
french.AssocFileExtension=&Associer %1 avec l'extension de fichier %2
french.AssocingFileExtension=Associe %1 avec l'extension de fichier %2...
french.AutoStartProgramGroupDescription=Démarrage :
french.AutoStartProgram=Démarrer automatiquement %1
french.AddonHostProgramNotFound=%1 n'a pas été trouvé dans le dossier que vous avez choisi.%n%nVoulez-vous continuer malgré tout ?
german.NameAndVersion=%1 Version %2
german.AdditionalIcons=Zusätzliche Symbole:
german.CreateDesktopIcon=&Desktop-Symbol erstellen
german.CreateQuickLaunchIcon=Symbol in der Schnellstartleiste erstellen
german.ProgramOnTheWeb=%1 im Internet
german.UninstallProgram=%1 entfernen
german.LaunchProgram=%1 starten
german.AssocFileExtension=&Registriere %1 mit der %2-Dateierweiterung
german.AssocingFileExtension=%1 wird mit der %2-Dateierweiterung registriert...
german.AutoStartProgramGroupDescription=Beginn des Setups:
german.AutoStartProgram=Starte automatisch%1
german.AddonHostProgramNotFound=%1 konnte im ausgewählten Ordner nicht gefunden werden.%n%nMöchten Sie dennoch fortfahren?
hebrew.NameAndVersion=%1 גירסה %2
hebrew.AdditionalIcons=סימלונים נוספים:
hebrew.CreateDesktopIcon=צור קיצור דרך על &שולחן העבודה
hebrew.CreateQuickLaunchIcon=צור סימלון בשורת ההרצה המהירה
hebrew.ProgramOnTheWeb=%1 ברשת
hebrew.UninstallProgram=הסר את %1
hebrew.LaunchProgram=הפעל %1
hebrew.AssocFileExtension=&קשר את %1 עם סיומת הקובץ %2
hebrew.AssocingFileExtension=מקשר את %1 עם סיומת הקובץ %2
hebrew.AutoStartProgramGroupDescription=הפעלה אוטומטית:
hebrew.AutoStartProgram=הפעל אוטומטית %1
hebrew.AddonHostProgramNotFound=%1 לא נמצא בתיקיה שבחרת.%n%nאתה רוצה להמשיך בכל זאת?
italian.NameAndVersion=%1 versione %2
italian.AdditionalIcons=Icone aggiuntive:
italian.CreateDesktopIcon=Crea un'icona sul &desktop
italian.CreateQuickLaunchIcon=Crea un'icona nella &barra 'Avvio veloce'
italian.ProgramOnTheWeb=Sito web di %1
italian.UninstallProgram=Disinstalla %1
italian.LaunchProgram=Avvia %1
italian.AssocFileExtension=&Associa i file con estensione %2 a %1
italian.AssocingFileExtension=Associazione dei file con estensione %2 a %1...
italian.AutoStartProgramGroupDescription=Esecuzione automatica:
italian.AutoStartProgram=Esegui automaticamente %1
italian.AddonHostProgramNotFound=Impossibile individuare %1 nella cartella selezionata.%n%nVuoi continuare ugualmente?
japanese.NameAndVersion=%1 バージョン %2
japanese.AdditionalIcons=アイコンを追加する:
japanese.CreateDesktopIcon=デスクトップ上にアイコンを作成する(&D)
japanese.CreateQuickLaunchIcon=クイック起動アイコンを作成する(&Q)
japanese.ProgramOnTheWeb=%1 on the Web
japanese.UninstallProgram=%1 をアンインストールする
japanese.LaunchProgram=%1 を実行する
japanese.AssocFileExtension=ファイル拡張子 %2 に %1 を関連付けます。
japanese.AssocingFileExtension=ファイル拡張子 %2 に %1 を関連付けています...
japanese.AutoStartProgramGroupDescription=スタートアップ:
japanese.AutoStartProgram=%1 を自動的に開始する
japanese.AddonHostProgramNotFound=選択されたフォルダーに %1 が見つかりませんでした。%n%nこのまま続行しますか？
norwegian.NameAndVersion=%1 versjon %2
norwegian.AdditionalIcons=Ekstra-ikoner:
norwegian.CreateDesktopIcon=Lag ikon på &skrivebordet
norwegian.CreateQuickLaunchIcon=Lag et &Hurtigstarts-ikon
norwegian.ProgramOnTheWeb=%1 på nettet
norwegian.UninstallProgram=Avinstaller %1
norwegian.LaunchProgram=Kjør %1
norwegian.AssocFileExtension=&Koble %1 med filetternavnet %2
norwegian.AssocingFileExtension=Kobler %1 med filetternavnet %2...
norwegian.AutoStartProgramGroupDescription=Oppstart:
norwegian.AutoStartProgram=Start %1 automatisk
norwegian.AddonHostProgramNotFound=%1 ble ikke funnet i katalogen du valgte.%n%nVil du fortsette likevel?
polish.NameAndVersion=%1 (wersja %2)
polish.AdditionalIcons=Dodatkowe skróty:
polish.CreateDesktopIcon=Utwórz skrót na &pulpicie
polish.CreateQuickLaunchIcon=Utwórz skrót na pasku &szybkiego uruchamiania
polish.ProgramOnTheWeb=Strona internetowa aplikacji %1
polish.UninstallProgram=Dezinstalacja aplikacji %1
polish.LaunchProgram=Uruchom aplikację %1
polish.AssocFileExtension=&Przypisz aplikację %1 do rozszerzenia pliku %2
polish.AssocingFileExtension=Przypisywanie aplikacji %1 do rozszerzenia pliku %2...
polish.AutoStartProgramGroupDescription=Autostart:
polish.AutoStartProgram=Automatycznie uruchamiaj aplikację %1
polish.AddonHostProgramNotFound=Aplikacja %1 nie została znaleziona we wskazanym przez Ciebie folderze.%n%nCzy pomimo tego chcesz kontynuować?
portuguese.NameAndVersion=%1 versão %2
portuguese.AdditionalIcons=Atalhos adicionais:
portuguese.CreateDesktopIcon=Criar atalho no Ambiente de &Trabalho
portuguese.CreateQuickLaunchIcon=&Criar atalho na barra de Iniciação Rápida
portuguese.ProgramOnTheWeb=%1 na Web
portuguese.UninstallProgram=Desinstalar o %1
portuguese.LaunchProgram=Executar o %1
portuguese.AssocFileExtension=Associa&r o %1 aos ficheiros com a extensão %2
portuguese.AssocingFileExtension=A associar o %1 aos ficheiros com a extensão %2...
portuguese.AutoStartProgramGroupDescription=Inicialização Automática:
portuguese.AutoStartProgram=Iniciar %1 automaticamente
portuguese.AddonHostProgramNotFound=Não foi possível localizar %1 na pasta seleccionada.%n%nDeseja continuar de qualquer forma?
russian.NameAndVersion=%1, версия %2
russian.AdditionalIcons=Дополнительные значки:
russian.CreateDesktopIcon=Создать значок на &Рабочем столе
russian.CreateQuickLaunchIcon=Создать значок в &Панели быстрого запуска
russian.ProgramOnTheWeb=Сайт %1 в Интернете
russian.UninstallProgram=Деинсталлировать %1
russian.LaunchProgram=Запустить %1
russian.AssocFileExtension=Св&язать %1 с файлами, имеющими расширение %2
russian.AssocingFileExtension=Связывание %1 с файлами %2...
russian.AutoStartProgramGroupDescription=Автозапуск:
russian.AutoStartProgram=Автоматически запускать %1
russian.AddonHostProgramNotFound=%1 не найден в указанной вами папке.%n%nВы всё равно хотите продолжить?
slovenian.NameAndVersion=%1 različica %2
slovenian.AdditionalIcons=Dodatne ikone:
slovenian.CreateDesktopIcon=Ustvari ikono na &namizju
slovenian.CreateQuickLaunchIcon=Ustvari ikono za &hitri zagon
slovenian.ProgramOnTheWeb=%1 na spletu
slovenian.UninstallProgram=Odstrani %1
slovenian.LaunchProgram=Odpri %1
slovenian.AssocFileExtension=&Poveži %1 s pripono %2
slovenian.AssocingFileExtension=Povezujem %1 s pripono %2...
slovenian.AutoStartProgramGroupDescription=Zagon:
slovenian.AutoStartProgram=Samodejno zaženi %1
slovenian.AddonHostProgramNotFound=Programa %1 ni bilo mogoče najti v izbrani mapi.%n%nAli želite vseeno nadaljevati?
spanish.NameAndVersion=%1 versión %2
spanish.AdditionalIcons=Accesos directos adicionales:
spanish.CreateDesktopIcon=Crear un acceso directo en el &escritorio
spanish.CreateQuickLaunchIcon=Crear un acceso directo en &Inicio Rápido
spanish.ProgramOnTheWeb=%1 en la Web
spanish.UninstallProgram=Desinstalar %1
spanish.LaunchProgram=Ejecutar %1
spanish.AssocFileExtension=&Asociar %1 con la extensión de archivo %2
spanish.AssocingFileExtension=Asociando %1 con la extensión de archivo %2...
spanish.AutoStartProgramGroupDescription=Inicio:
spanish.AutoStartProgram=Iniciar automáticamente %1
spanish.AddonHostProgramNotFound=%1 no pudo ser localizado en la carpeta seleccionada.%n%n¿Desea continuar de todas formas?
turkish.NameAndVersion=%1 %2 sürümü
turkish.AdditionalIcons=Ek simgeler:
turkish.CreateDesktopIcon=Masaüstü simg&esi oluşturulsun
turkish.CreateQuickLaunchIcon=Hızlı Başlat simgesi &oluşturulsun
turkish.ProgramOnTheWeb=%1 Web Sitesi
turkish.UninstallProgram=%1 Uygulamasını Kaldır
turkish.LaunchProgram=%1 Uygulamasını Çalıştır
turkish.AssocFileExtension=%1 &uygulaması ile %2 dosya uzantısı ilişkilendirilsin
turkish.AssocingFileExtension=%1 uygulaması ile %2 dosya uzantısı ilişkilendiriliyor...
turkish.AutoStartProgramGroupDescription=Başlangıç:
turkish.AutoStartProgram=%1 otomatik olarak başlatılsın
turkish.AddonHostProgramNotFound=%1 seçtiğiniz klasörde bulunamadı.%n%nYine de devam etmek istiyor musunuz?
ukrainian.NameAndVersion=%1, версія %2
ukrainian.AdditionalIcons=Додаткові ярлики:
ukrainian.CreateDesktopIcon=Створити ярлики на &Робочому столі
ukrainian.CreateQuickLaunchIcon=Створити ярлики на &Панелі швидкого запуску
ukrainian.ProgramOnTheWeb=Сайт %1 в Інтернеті
ukrainian.UninstallProgram=Видалити %1
ukrainian.LaunchProgram=Відкрити %1
ukrainian.AssocFileExtension=&Асоціювати %1 з розширенням файлу %2
ukrainian.AssocingFileExtension=Асоціювання %1 з розширенням файлу %2...
ukrainian.AutoStartProgramGroupDescription=Автозавантаження:
ukrainian.AutoStartProgram=Автоматично завантажувати %1
ukrainian.AddonHostProgramNotFound=%1 не знайдений у вказаній вами папці%n%nВи все одно бажаєте продовжити?

[Languages]
; These files are stubs
; To achieve better results after recompilation, use the real language files
Name: "english"; MessagesFile: "embedded\english.isl"; 
Name: "brazilianportuguese"; MessagesFile: "embedded\brazilianportuguese.isl"; 
Name: "catalan"; MessagesFile: "embedded\catalan.isl"; 
Name: "corsican"; MessagesFile: "embedded\corsican.isl"; 
Name: "czech"; MessagesFile: "embedded\czech.isl"; 
Name: "danish"; MessagesFile: "embedded\danish.isl"; 
Name: "dutch"; MessagesFile: "embedded\dutch.isl"; 
Name: "finnish"; MessagesFile: "embedded\finnish.isl"; 
Name: "french"; MessagesFile: "embedded\french.isl"; 
Name: "german"; MessagesFile: "embedded\german.isl"; 
Name: "hebrew"; MessagesFile: "embedded\hebrew.isl"; 
Name: "italian"; MessagesFile: "embedded\italian.isl"; 
Name: "japanese"; MessagesFile: "embedded\japanese.isl"; 
Name: "norwegian"; MessagesFile: "embedded\norwegian.isl"; 
Name: "polish"; MessagesFile: "embedded\polish.isl"; 
Name: "portuguese"; MessagesFile: "embedded\portuguese.isl"; 
Name: "russian"; MessagesFile: "embedded\russian.isl"; 
Name: "slovenian"; MessagesFile: "embedded\slovenian.isl"; 
Name: "spanish"; MessagesFile: "embedded\spanish.isl"; 
Name: "turkish"; MessagesFile: "embedded\turkish.isl"; 
Name: "ukrainian"; MessagesFile: "embedded\ukrainian.isl"; 
