# c4-images.sh c4ix|bare -- The Ace of Penguins' part of c4's build-images.sh,
# which sources it from c4's root (games/README.md in c4). Each game is its
# own program, with lib/ and its images inside it: there is no data to put on
# a disk (games/ace/README.c4.md says why). ACE_GAMES and each game's extra
# units are as c4.mk has them. build-images.sh sources this inside its own
# loop over $g, so the loops here have a name of their own.
ACE_GAMES="solitaire canfield freecell golf mastermind merlin minesweeper pegged spider taipei thornq"
ace_objs () {
    echo .ace/g/$1.c4o .ace/g/$1-img.c4o .ace/g/$1-help.c4o .ace/g/$1-demo.c4o \
        .ace/lib/table.c4o .ace/lib/help.c4o .ace/lib/stack.c4o .ace/lib/imagelib.c4o \
        .ace/lib/funcs.c4o .ace/lib/table_rn.c4o .ace/lib/xwin-c4.c4o .ace/lib/cards-img.c4o
    if [ "$1" = taipei ]; then echo .ace/g/taipeilib.c4o; fi
}
case "$1" in
c4ix)
    if ls .ace/g/*.c4o > /dev/null 2>&1 && [ -f $PB/lib/libc.c4s ]; then
        say "linking The Ace of Penguins for C4IX"
        # build-images.sh takes this repository's block of the manifest if
        # c4ix-ace.c4r is on the disk, so one of them has that name
        ./c4rlink32 $(ace_objs solitaire) .ace/plat/plat-c4ix.c4o -l $PB/lib/libc.c4s -o $DISK/c4ix-ace.c4r > /dev/null
        for ace_g in $ACE_GAMES; do
            if [ $ace_g != solitaire ]; then
                ./c4rlink32 $(ace_objs $ace_g) .ace/plat/plat-c4ix.c4o -l $PB/lib/libc.c4s -o $DISK/c4ix-ace-$ace_g.c4r > /dev/null
            fi
        done
    fi ;;
bare)
    if ls .ace/g/*.c4o > /dev/null 2>&1; then
        for ace_g in $ACE_GAMES; do
            item $ace_g
            ./c4rlink32 $(ace_objs $ace_g) .ace/plat/plat-bare.c4o .libc-bare/libc-*.c4o -o $GAMESDISK/$ace_g.c4r > /dev/null
        done
        GAMES_ON_DISK="$GAMES_ON_DISK SOLITAIRE FREECELL"
    fi ;;
esac
