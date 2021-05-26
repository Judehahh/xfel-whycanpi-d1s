#include <fel.h>

static int file_save(const char * filename, void * buf, size_t len)
{
	FILE * out = fopen(filename, "wb");
	int r;
	if(!out)
	{
		perror("Failed to open output file");
		exit(-1);
	}
	r = fwrite(buf, len, 1, out);
	fclose(out);
	return r;
}

static void * file_load(const char * filename, size_t * len)
{
	size_t offset = 0, bufsize = 8192;
	char * buf = malloc(bufsize);
	FILE * in;
	if(strcmp(filename, "-") == 0)
		in = stdin;
	else
		in = fopen(filename, "rb");
	if(!in)
	{
		perror("Failed to open input file");
		exit(-1);
	}
	while(1)
	{
		size_t len = bufsize - offset;
		size_t n = fread(buf + offset, 1, len, in);
		offset += n;
		if(n < len)
			break;
		bufsize *= 2;
		buf = realloc(buf, bufsize);
		if(!buf)
		{
			perror("Failed to resize load_file() buffer");
			exit(-1);
		}
	}
	if(len)
		*len = offset;
	if(in != stdin)
		fclose(in);
	return buf;
}

static void hexdump(uint32_t addr, void * buf, size_t len)
{
	unsigned char * p = buf;
	size_t i, j;

	for(j = 0; j < len; j += 16)
	{
		printf("%08zx: ", addr + j);
		for(i = 0; i < 16; i++)
		{
			if(j + i < len)
				printf("%02x ", p[j + i]);
			else
				printf("   ");
		}
		putchar(' ');
		for(i = 0; i < 16; i++)
		{
			if(j + i >= len)
				putchar(' ');
			else
				putchar(isprint(p[j + i]) ? p[j + i] : '.');
		}
		printf("\r\n");
	}
}

static void usage(void)
{
	printf("xfel-v1.0.0 https://github.com/xboot/xfel\r\n");
	printf("usage:\r\n");
	printf("    xfel help                                   - Print this usage summary\r\n");
	printf("    xfel version                                - Show brom version\r\n");
	printf("    xfel hexdump <address> <length>             - Dumps memory region in hex\r\n");
	printf("    xfel dump <address> <length>                - Binary memory dump to stdout\r\n");
	printf("    xfel exec <address>                         - Call function address\r\n");
	printf("    xfel read32 <address>                       - Read 32-bits value from device memory\r\n");
	printf("    xfel write32 <address> <value>              - Write 32-bits value to device memory\r\n");
	printf("    xfel read <address> <length> <file>         - Read memory to file\r\n");
	printf("    xfel write <address> <file>                 - Write file to memory\r\n");
	printf("    xfel reset                                  - Reset device using watchdog\r\n");
	printf("    xfel sid                                    - Output 128-bits SID information\r\n");
	printf("    xfel jtag                                   - Enable JTAG debug\r\n");
	printf("    xfel ddr [type]                             - Initial DDR controller with optional type\r\n");
	printf("    xfel spinor                                 - Detect spi nor flash\r\n");
	printf("    xfel spinor read <address> <length> <file>  - Read spi nor flash to file\r\n");
	printf("    xfel spinor write <address> <file>          - Write file to spi nor flash\r\n");
	printf("    xfel spinand                                - Detect spi nand flash\r\n");
	printf("    xfel spinand read <address> <length> <file> - Read spi nand flash to file\r\n");
	printf("    xfel spinand write <address> <file>         - Write file to spi nand flash\r\n");
}

int main(int argc, char * argv[])
{
	struct xfel_ctx_t ctx;

	if(argc < 2)
	{
		usage();
		return -1;
	}
	libusb_init(NULL);
	ctx.hdl = libusb_open_device_with_vid_pid(NULL, 0x1f3a, 0xefe8);
	if(!fel_init(&ctx))
	{
		printf("ERROR: Can't found any FEL device\r\n");
		if(ctx.hdl)
			libusb_close(ctx.hdl);
		libusb_exit(NULL);
		return -1;
	}
	if(!ctx.chip)
	{
		printf("WARNNING: Not yet support this device\r\n");
		printf("%.8s soc=%08x %08x ver=%04x %02x %02x scratchpad=%08x %08x %08x\r\n",
			ctx.version.signature, ctx.version.id, ctx.version.unknown_0a,
			ctx.version.protocol, ctx.version.unknown_12, ctx.version.unknown_13,
			ctx.version.scratchpad, ctx.version.pad[0], ctx.version.pad[1]);
		if(ctx.hdl)
			libusb_close(ctx.hdl);
		libusb_exit(NULL);
		return -1;
	}
	if(!strcmp(argv[1], "help"))
	{
		usage();
	}
	else if(!strcmp(argv[1], "version"))
	{
		printf("%.8s soc=%08x(%s) %08x ver=%04x %02x %02x scratchpad=%08x %08x %08x\r\n",
			ctx.version.signature, ctx.version.id, ctx.chip->name, ctx.version.unknown_0a,
			ctx.version.protocol, ctx.version.unknown_12, ctx.version.unknown_13,
			ctx.version.scratchpad, ctx.version.pad[0], ctx.version.pad[1]);
	}
	else if(!strcmp(argv[1], "hexdump"))
	{
		argc -= 2;
		argv += 2;
		if(argc == 2)
		{
			uint32_t addr = strtoul(argv[0], NULL, 0);
			size_t len = strtoul(argv[1], NULL, 0);
			char * buf = malloc(len);
			if(buf)
			{
				fel_read(&ctx, addr, buf, len, 0);
				hexdump(addr, buf, len);
				free(buf);
			}
		}
		else
			usage();
	}
	else if(!strcmp(argv[1], "dump"))
	{
		argc -= 2;
		argv += 2;
		if(argc == 2)
		{
			uint32_t addr = strtoul(argv[0], NULL, 0);
			size_t len = strtoul(argv[1], NULL, 0);
			char * buf = malloc(len);
			if(buf)
			{
				fel_read(&ctx, addr, buf, len, 0);
				fwrite(buf, len, 1, stdout);
				free(buf);
			}
		}
		else
			usage();
	}
	else if(!strcmp(argv[1], "exec"))
	{
		argc -= 2;
		argv += 2;
		if(argc == 1)
		{
			uint32_t addr = strtoul(argv[0], NULL, 0);
			fel_exec(&ctx, addr);
		}
		else
			usage();
	}
	else if(!strcmp(argv[1], "read32"))
	{
		argc -= 2;
		argv += 2;
		if(argc == 1)
		{
			uint32_t addr = strtoul(argv[0], NULL, 0);
			printf("0x%08x\r\n", fel_read32(&ctx, addr));
		}
		else
			usage();
	}
	else if(!strcmp(argv[1], "write32"))
	{
		argc -= 2;
		argv += 2;
		if(argc == 2)
		{
			uint32_t addr = strtoul(argv[0], NULL, 0);
			size_t val = strtoul(argv[1], NULL, 0);
			fel_write32(&ctx, addr, val);
		}
		else
			usage();
	}
	else if(!strcmp(argv[1], "read"))
	{
		argc -= 2;
		argv += 2;
		if(argc == 3)
		{
			uint32_t addr = strtoul(argv[0], NULL, 0);
			size_t len = strtoul(argv[1], NULL, 0);
			char * buf = malloc(len);
			if(buf)
			{
				fel_read(&ctx, addr, buf, len, 1);
				file_save(argv[2], buf, len);
				free(buf);
			}
		}
		else
			usage();
	}
	else if(!strcmp(argv[1], "write"))
	{
		argc -= 2;
		argv += 2;
		if(argc == 2)
		{
			uint32_t addr = strtoul(argv[0], NULL, 0);
			size_t len;
			void * buf = file_load(argv[1], &len);
			if(buf)
			{
				fel_write(&ctx, addr, buf, len, 1);
				free(buf);
			}
		}
		else
			usage();
	}
	else if(!strcmp(argv[1], "reset"))
	{
		if(!fel_chip_reset(&ctx))
			printf("The chip don't support '%s' command\r\n", argv[1]);
	}
	else if(!strcmp(argv[1], "sid"))
	{
		uint32_t sid[4];
		if(fel_chip_sid(&ctx, sid))
			printf("%08x%08x%08x%08x\r\n",sid[0], sid[1], sid[2], sid[3]);
		else
			printf("The chip don't support '%s' command\r\n", argv[1]);
	}
	else if(!strcmp(argv[1], "jtag"))
	{
		if(!fel_chip_jtag(&ctx))
			printf("The chip don't support '%s' command\r\n", argv[1]);
	}
	else if(!strcmp(argv[1], "ddr"))
	{
		argc -= 2;
		argv += 2;
		if(!fel_chip_ddr(&ctx, (argc == 1) ? argv[0] : NULL))
			printf("The chip don't support '%s' command\r\n", argv[1]);
	}
	else if(!strcmp(argv[1], "spinor"))
	{
		argc -= 2;
		argv += 2;
		if(argc == 0)
		{
			if(fel_chip_spinor(&ctx))
				printf("Found spi nor flash\r\n");
			else
				printf("Not found any spi nor flash\r\n");
		}
		else
		{
			if(!strcmp(argv[0], "read"))
			{
				argc -= 1;
				argv += 1;
				uint32_t addr = strtoul(argv[0], NULL, 0);
				size_t len = strtoul(argv[1], NULL, 0);
				char * buf = malloc(len);
				if(buf)
				{
					fel_chip_spinor_read(&ctx, addr, buf, len);
					file_save(argv[2], buf, len);
					free(buf);
				}
			}
			else if(!strcmp(argv[0], "write"))
			{
				argc -= 1;
				argv += 1;
				uint32_t addr = strtoul(argv[0], NULL, 0);
				size_t len;
				void * buf = file_load(argv[1], &len);
				if(buf)
				{
					fel_chip_spinor_write(&ctx, addr, buf, len);
					free(buf);
				}
			}
			else
				usage();
		}
	}
	else if(!strcmp(argv[1], "spinand"))
	{
		argc -= 2;
		argv += 2;
		if(argc == 0)
		{
			if(fel_chip_spinand(&ctx))
				printf("Found spi nand flash\r\n");
			else
				printf("Not found any spi nand flash\r\n");
		}
		else
		{
			if(!strcmp(argv[0], "read"))
			{
				argc -= 1;
				argv += 1;
				uint32_t addr = strtoul(argv[0], NULL, 0);
				size_t len = strtoul(argv[1], NULL, 0);
				char * buf = malloc(len);
				if(buf)
				{
					fel_chip_spinand_read(&ctx, addr, buf, len);
					file_save(argv[2], buf, len);
					free(buf);
				}
			}
			else if(!strcmp(argv[0], "write"))
			{
				argc -= 1;
				argv += 1;
				uint32_t addr = strtoul(argv[0], NULL, 0);
				size_t len;
				void * buf = file_load(argv[1], &len);
				if(buf)
				{
					fel_chip_spinand_write(&ctx, addr, buf, len);
					free(buf);
				}
			}
			else
				usage();
		}
	}
	else
	{
		usage();
	}
	if(ctx.hdl)
		libusb_close(ctx.hdl);
	libusb_exit(NULL);

	return 0;
}
