C:
cd C:\OHWorkspace\quakevr\QC

:: definitely crashes
:: .\fteqcc64.exe -O3 -Fautoproto -progdefs -Olo -Fiffloat -Fifvector -Fvectorlogic

:: definitely crashes
:: .\fteqcc64.exe -O3 -Fautoproto -progdefs -Olo -Fiffloat -Fifvector -Fvectorlogic

:: definitely crashes
:: .\fteqcc64.exe -O3 -Fautoproto -progdefs

:: definitely crashes
:: .\fteqcc64.exe -O1 -Fautoproto -progdefs -Olo -Fiffloat -Fifvector -Fvectorlogic

:: definitely crashes
:: .\fteqcc64.exe -O1 -Fautoproto -progdefs -Olo

:: definitely crashes
.\fteqcc64.exe -O3 -Fautoproto -progdefs -Fiffloat -Fifvector -Fvectorlogic -Flo -Fsubscope -Wall -Wextra -Wno-F209 -Wno-F208


:: doesn't seem to crash - best version
:: .\fteqcc64.exe -O3 -Fautoproto -progdefs -Ono-lo -Fiffloat -Fifvector -Fvectorlogic -Flo -Fsubscope -Wall -Wextra -Wno-F209 -Wno-F208
:: -DNDEBUG=1

:: doesn't seem to crash - best version (no -Flo)
:: .\fteqcc64.exe -O3 -Fautoproto -progdefs -Ono-lo -Fiffloat -Fifvector -Fvectorlogic

:: doesn't seem to crash
:: .\fteqcc64.exe -O3 -Fautoproto -progdefs -Ono-lo

:: doesn't seem to crash
::.\fteqcc64.exe -O1 -Fautoproto -progdefs -Fiffloat -Fifvector -Fvectorlogic

:: doesn't seem to crash
:: .\fteqcc64.exe -O1 -Fautoproto -progdefs

:: doesn't seem to crash
:: .\fteqcc64.exe -Fautoproto -progdefs

.\pak.exe -c -v pak11.pak vrprogs.dat
xcopy /y C:\OHWorkspace\quakevr\QC\pak11.pak C:\OHWorkspace\quakevr\ReleaseFiles\Id1
:: echo F | xcopy /y C:\OHWorkspace\quakevr\QC\progdefs.h C:\OHWorkspace\quakevr\Quake\progdefs_generated.hpp

