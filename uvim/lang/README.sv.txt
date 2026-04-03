README.txt för version 9.1 av MNV: MNV is not Vim.


VAD ÄR MNV?

MNV är en kraftigt förbättrad version av den gamla goda UNIX-editorn Vi.  Många nya
funktioner har lagts till: ångra på flera nivåer, syntaxmarkering, kommandoradshistorik
historik, onlinehjälp, stavningskontroll, filnamns komplettering, blockoperationer,
skriptspråk etc.  Det finns också ett grafiskt användargränssnitt (GUI) tillgängligt.
Vi-kompatibiliteten bibehålls dock, så de som har Vi "i fingrarna" kommer
känna sig som hemma.  Se "runtime/doc/vi_diff.txt" för skillnader jämfört med Vi.

Denna editor är mycket användbar för att redigera program och andra vanliga textfiler.
Alla kommandon ges med vanliga tangentbordstecken, så de som kan skriva
med tio fingrar kan arbeta mycket snabbt.  Dessutom kan funktionsknapparna
mappas till kommandon av användaren, och musen kan användas.

MNV syftar också till att tillhandahålla en (mestadels) POSIX-kompatibel vi-implementering när
kompileras med en minimal uppsättning funktioner (vanligtvis kallad mnv.tiny), som används
av många Linux-distributioner som standardvi-redigerare.

MNV körs under MS-Windows (7, 8, 10, 11), macOS, Haiku, VMS och nästan alla
varianter av UNIX.  Det bör inte vara särskilt svårt att porta till andra system.
Äldre versioner av MNV körs på MS-DOS, MS-Windows 95/98/Me/NT/2000/XP/Vista,
Amiga DOS, Atari MiNT, BeOS, RISC OS och OS/2.  Dessa underhålls inte längre.


DISTRIBUTION

Du kan ofta använda din favoritpakethanterare för att installera MNV.  På Mac och
Linux är en liten version av MNV förinstallerad, men du behöver ändå installera MNV
om du vill ha fler funktioner.

Det finns separata distributioner för Unix, PC, Amiga och vissa andra system.
Denna README.txt-fil medföljer runtime-arkivet.  Den innehåller
dokumentation, syntaxfiler och andra filer som används vid körning.  För att köra
MNV måste du skaffa antingen ett av binärarkiven eller ett källarkiv.
Vilket du behöver beror på vilket system du vill köra det på och om du
vill eller måste kompilera det själv.  Se "https://www.mnv.org/download.php" för
en översikt över de distributioner som för närvarande finns tillgängliga.

Några populära ställen att hämta den senaste versionen av MNV:
* Kolla in git-arkivet från github: https://github.com/Project-Tick/Project-Tick.
* Hämta källkoden som ett arkiv: https://github.com/Project-Tick/Project-Tick/tags.
* Hämta en Windows-körbar fil från mnv-win32-installer-arkivet:
  https://github.com/Project-Tick/Project-Tick-win32-installer/releases.


KOMPILERING

Om du har skaffat en binär distribution behöver du inte kompilera MNV.  Om du
har skaffat en källkodsdistribution finns allt du behöver för att kompilera MNV i
katalogen "src".  Se src/INSTALL för instruktioner.


INSTALLATION

Se någon av dessa filer för systemspecifika instruktioner.  Antingen i
READMEdir-katalogen (i arkivet) eller i toppkatalogen (om du packar upp en
arkiv):

README_ami.txt        Amiga
README_unix.txt       Unix
README_dos.txt        MS-DOS och MS-Windows
README_mac.txt        Macintosh
README_haiku.txt      Haiku
README_vms.txt        VMS

Det finns andra README_*.txt-filer, beroende på vilken distribution du använde.


DOKUMENTATION

MNV-tutorn är en timmes lång utbildningskurs för nybörjare.  Ofta kan den
startas som "mnvtutor".  Se ":help tutor" för mer information.

Det bästa är att använda ":help" i MNV.  Om du ännu inte har en körbar fil, läs
"runtime/doc/help.txt".  Den innehåller hänvisningar till andra dokumentationsfiler.
Användarhandboken läses som en bok och rekommenderas för att lära sig använda MNV.  Se
":help user-manual".


KOPIERING

MNV är Charityware.  Du kan använda och kopiera det så mycket du vill, men du
uppmuntras att göra en donation för att hjälpa föräldralösa barn i Uganda.  Läs filen
"runtime/doc/uganda.txt" för mer information (skriv ":help uganda" i MNV).

Sammanfattning av licensen: Det finns inga begränsningar för användning eller distribution av en
oförändrad kopia av MNV.  Delar av MNV får också distribueras, men licenstexten
texten måste alltid inkluderas.  För modifierade versioner gäller några begränsningar.
Licensen är GPL-kompatibel, du kan kompilera MNV med GPL-bibliotek och
distribuera det.


SPONSRING

Att fixa buggar och lägga till nya funktioner tar mycket tid och ansträngning.  För att visa
din uppskattning för arbetet och motivera utvecklarna att fortsätta arbeta med
MNV, skicka gärna en donation.

Pengarna du donerar kommer huvudsakligen att användas för att hjälpa barn i Uganda.  Se
"runtime/doc/uganda.txt".  Men samtidigt ökar donationerna
utvecklingsteamets motivation att fortsätta arbeta med MNV!

För den senaste informationen om sponsring, se MNVs webbplats:
	https://www.mnv.org/sponsor/


BIDRA

Om du vill hjälpa till att förbättra MNV, se filen CONTRIBUTING.md.


INFORMATION

Om du använder macOS kan du använda MacMNV: https://macmnv.org

De senaste nyheterna om MNV finns på MNVs hemsida:
	https://www.mnv.org/

Om du har problem, ta en titt på MNVs dokumentation eller tips:
	https://www.mnv.org/docs.php
	https://mnv.fandom.com/wiki/MNV_Tips_Wiki

Om du fortfarande har problem eller andra frågor, använd någon av mailinglistorna
för att diskutera dem med MNV-användare och utvecklare:
	https://www.mnv.org/maillist.php

Om inget annat fungerar, rapportera buggar direkt till mnv-dev-maillistan:
	<mnv-dev@mnv.org>


HUVUDFÖRFATTARE

Det mesta av MNV har skapats av Bram Moolenaar <Bram@mnv.org>, ":help Bram-Moolenaar"

Skicka övriga kommentarer, patchar, blommor och förslag till mnv-dev
: <mnv-dev@mnv.org>
