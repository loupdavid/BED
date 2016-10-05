all: tp1.elf

tp1.elf: tp1.s
	msp430-gcc -mmcu=msp430f2274 -mdisable-watchdog -o tp1.elf tp1.s

download:
	mspdebug rf2500
