.section .init9

main:
  /* Init de la diode rouge et verte */
  mov.b #3, &0x22
  mov.b #0, &0x26


loop:
  /* Eteindre la diode rouge */
  mov.b #0, &0x21

  mov #50000, R4
wait0:
  dec R4
  jnz wait0

  /* Allumer la diode rouge */
  mov.b #1, &0x21

  mov #50000, R4
wait1:
  dec R4
  jnz wait1

jmp loop
