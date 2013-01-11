gpt: gpt.c crc32.c
	gcc gpt.c crc32.c -o gpt
clean:
	rm -rf gpt
