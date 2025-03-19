![image](https://cloud.githubusercontent.com/assets/6544511/22624161/934dba64-eb27-11e6-8f78-46c902e96e1b.png)
========

### Chiri's wrapper to enable fixing broken stereoscopic effects in DX11 games.
### Adjusted for use with [Stella Mod](https://stella.sefinek.net)

This includes the entire code base, and it will compile, link, and run in it's current state.

This is not the end-user version of the tool, this is for people developing the code by fixing
bugs, adding new features, or documenting how to use it.

The current project is set up using Visual Studio 2017 Community, so anyone can do development for free.

### To get started do:

1. Install IE 10 or 11. VS2017 apparently requires this, but might have been fixed recently.
2. Download VS2017 Community for Windows Desktop.
3. Install VS2017 and be sure to select:
    - Programming Languages -> Visual C++
    - Windows 11 SDK (10.0.22621.0)
4. Run VS2017.
5. TEAM menu, Connect. Opens the Connect page for cloning.
6. Use Clone menu, and enter the repository:
   https://github.com/sefinek/3DMigoto.git
7. Change the source-code destination to where you prefer, and then click Clone.
8. Double-click your new local repository to set it active (if you have others.)
9. At the home menu in Team Explorer, double click StereovisionHacks.sln to open the solution.
10. Switch to Solution Explorer, and wait for it to parse all the files.
11. Hit F7 to build the full solution.
12. Output files are in .\x64\Debug (3 dll and 1 .ini)

### If you have any questions or problems don't hesitate to contact me.

Big, big, _impossibly_ big thanks to Chiri for open-sourcing 3DMigoto.
