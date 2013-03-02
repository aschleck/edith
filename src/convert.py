import sys

input = open("../build/sendtables.txt").read()

at = 0
while at < len(input):
  if input[at] == "\\":
    if input[at + 1].isdigit():
      char = int(input[at + 1:at + 4], 8)
      sys.stdout.write(chr(char))

      at += 4
    else:
      sys.stdout.write(input[at:at + 2].decode('string_escape'))
      
      at += 2
  else:
    sys.stdout.write(input[at])

    at += 1
