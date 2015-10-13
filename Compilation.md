# ARM9 Compilation #

  1. You need to get the last svn version of [pjsip](http://pjsip.org).
  1. You need to download the [patch](http://svsip.googlecode.com/files/patch%20-%20pjsip-nds%20-%2020090305.txt.zip) for pjsip
  1. Apply the patch
```
   patch -p0 < 'patch - pjsip-nds - 20090305.txt'
```
  1. Define in compile.sh the path of DEVKITPRO and DEVKITARM
  1. Compile
```
   ./compile.sh
```


# ARM7 Compilation #

In progress