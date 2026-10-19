// Package winres: the Windows resources of the launcher's own window (-tags wails,
// window_wails.go) -- its icon, its manifest (sharp at any display scaling, the
// common controls 6), its name in Windows' lists.  make builds
// rsrc_windows_amd64.syso here from madtv.rc (windres of MinGW; the icon from the
// game's pictures: openmtv icons build/icons) -- never kept in the repository; without
// it the program builds as well, only without them.
package winres
