@echo off

cmake -G "Visual Studio 15 2017 Win64" .. -DBOOST_ROOT=C:/local/boost_1_68_0 || exit /b 1

MSBuild TurtleCoin.sln /p:Configuration=Release /m || exit /b 1