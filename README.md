# simh <-> blinkenbone merge 

This repository with the only purpose of merging the
"realcons" features from
[BlinkenBone](https://github.com/j-hoppe/BlinkenBone/)
into the latest upstream version of
[simh](https://github.com/simh/simh).

Since this is for my personal use, I've chosen to only merge the PDP11
changes, since they are useful for use with
[PiDP-11](http://obsolescence.wixsite.com/obsolescence/pidp-11).

The build depends on two source directories included in the BlinkenBone
repository, and they are defined in the makefile as such:

```
BLINKENLIGHT_COMMON_DIR=../BlinkenBone/projects/00_common
BLINKENLIGHT_API_DIR=../BlinkenBone/projects/07.0_blinkenlight_api
```

To build against the [PiDP-11](https://github.com/PiDP/pidp11)
versions of these sources, change the paths accordingly:

```
BLINKENLIGHT_COMMON_DIR=../pidp11/src/00_common/
BLINKENLIGHT_API_DIR=../pidp11/src/07.0_blinkenlight_api/
```

Branches in this repository:

| Branch | Description |
| --- | --- |
| info | empty branch containing only this README |
| upstream | master branch of the upstream repository |
| realcons | master branch with the realcons changes merged |
