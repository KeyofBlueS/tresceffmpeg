# This project is authored by Eike Hein.
All credit for the implementation, design, and engineering belongs to them.

# TresCEFFmpeg
This project enables the videogame Trespasser to replace legacy .SMK video playback with modern FFmpeg-supported formats using a binary-compatible Smacker API replacement layer.

If running inside Trespasser CE, the DLL automatically applies runtime patches to force a 32-bit buffer for cutscene frames. Trespasser originally rendered Smacker video in 16-bit, and while CE extended the engine to use 32-bit for gameplay, it kept the legacy 16-bit path for cutscenes.

This removes that limitation, so you get not only higher-resolution playback, but also truecolor cutscenes instead of the original 16-bit output.

# Supported Formats
Through FFmpeg, the shim supports a wide range of modern and legacy media formats, including:

- Video codecs: H.264, H.265 (HEVC), Theora, VP8, VP9
- Audio codecs: MP3, Vorbis, AAC
- Containers: MP4, MKV, WebM
- Legacy support: Smacker (.SMK)

# Usage
- In the root folder of the Trespasser game, rename the original DLL:
`SMACKW32.DLL -> SMACKW32.DLL.bak`
- Place the downloaded smackw32.dll in the game root folder.
- Make a backup of the original .smk cutscenes located in:
`\data\menu\`
- Replace them with videos in any supported format.

**Important:** Keep the original filenames and .smk extension.
For example, if you want to replace the intro cutscene:

`somevideo.mp4 -> tpassintro.smk`
