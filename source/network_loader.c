#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ogcsys.h>
#include <gccore.h>
#include <network.h>
#include <errno.h>

#define RESET_DNS 1

#define NETWORK_LOADER_VERSION "1.2"

extern void __exception_closeall(void);
#include "elf_loader.h"
#include "fat.h"
#include "processor.h"
#include "sd.h"
#include "sync.h"

// =============== TAKEN FROM https://wiibrew.org/wiki//shared2/sys/net/02/config.dat =================

struct _proxy
{
    u8 use_proxy;               // 0x00 -> no proxy;  0x01 -> proxy
    u8 use_proxy_userandpass;   // 0x00 -> don't use username and password;  0x01 -> use username and password
    u8 padding_1[2];            // 0x00
    u8 proxy_name[255];
    u8 padding_2;               // 0x00
    u16 proxy_port;             // 0-34463 range
    u8 proxy_username[32];
    u8 padding_3;               // 0x00
    u8 proxy_password[32];
} __attribute__((__packed__));

typedef struct _proxy proxy_t;

typedef struct _connection
{
    /*
     *  Settings common to both wired and wireless connections
     */
    u8 flags;           // Defined below.
    u8 padding_1[3];

    u8 ip[4];           // Wii IP Address
    u8 netmask[4];
    u8 gateway[4];
    u8 dns1[4];
    u8 dns2[4];
    u8 padding_2[2];

    u16 mtu;            //valid values are 0 and 576-1500 range
    u8 padding_3[8];    // 0x00 padding?

    proxy_t proxy_settings;
    u8 padding_4;       //0x00

    proxy_t proxy_settings_copy;    // Seems to be a duplicate of proxy_settings
    u8 padding_5[1297];             //0x00

    /*
     *  Wireless specific settings
     */
    u8 ssid[32];        // Access Point name.

    u8 padding_6;       // 0x00
    u8 ssid_length;     // length of ssid[] (AP name) in bytes.
    u8 padding_7[2];    // 0x00

    u8 padding_8;       // 0x00
    u8 encryption;      // (Probably) Encryption.  OPN: 0x00, WEP64: 0x01, WEP128: 0x02 WPA-PSK (TKIP): 0x04, WPA2-PSK (AES): 0x05, WPA-PSK (AES): 0x06
    u8 padding_9[2];    // 0x00

    u8 padding_10;      // 0x00
    u8 key_length;      // length of key[] (encryption key) in bytes.  0x00 for WEP64 and WEP128.
    u8 unknown;         // 0x00 or 0x01 toogled with a WPA-PSK (TKIP) and with a WEP entered with hex instead of ascii.
    u8 padding_11;      // 0x00

    u8 key[64];         // Encryption key.  For WEP, key is stored 4 times (20 bytes for WEP64 and 52 bytes for WEP128) then padded with 0x00.

    u8 padding_12[236]; // 0x00
} connection_t;

typedef struct _netconfig
{
    u8 header0;     // 0x00
    u8 header1;     // 0x00
    u8 header2;     // 0x00
    u8 header3;     // 0x00
    u8 header4;     // 0x01  When there's at least one valid connection to the Internet.
    u8 header5;     // 0x00
    u8 header6;     // 0x07  Not sure.  This is always 0x07 for me (MetaFight)
    u8 header7;     // 0x00

    connection_t connection[3];
} netconfig_t;

// =============== END COPY PASTE ==============

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

void http_load();

void boot_elf(void *payload, size_t size);

void try_sd_load() {
	int err;
	
	err = sd_init();
	if (err) {
		printf("SD card not found (%d)\n", err);
		return;
	}
	
	err = fat_init();
	if (err == 0)
		printf("SD card detected\n");
	else {
		printf("SD card not detected (%d)\n", err);
		return;
	}
	
	printf("Opening boot.elf:\n");
	err = fat_open("boot.elf");
	
	if (err) {
		printf("boot.elf not found (%d)\n", err);
		return;
	}
	
extern u32 fat_file_size;
	
	printf("reading %d bytes...\n", fat_file_size);
	void *code_buffer = malloc(fat_file_size);
	if (code_buffer == NULL) {
		printf("Out of memory!\n");
		return;
	}
	err = fat_read(code_buffer, fat_file_size);
	if (err) {
		printf("Error %d reading file\n", err);
		return;
	}
	
	printf("Done.\n");
	sd_close();
	boot_elf(code_buffer, fat_file_size);
	return;
}

netconfig_t nc __attribute__((aligned(4096)));

int main(int argc, char **argv) {
	s32 ret;

	char localip[16] = {0};
	char gateway[16] = {0};
	char netmask[16] = {0};

	VIDEO_Init();

	rmode = VIDEO_GetPreferredMode(NULL);
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	console_init(xfb,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);

	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

	printf("\x1b[2;0H");
	printf("Network Loader v"NETWORK_LOADER_VERSION"\n");
	printf("Copyright 2018  Dexter Gerig\n");
	printf("Includes portions of savezelda\n");
#if RESET_DNS
	printf("Resetting DNS servers... ");
	
	s32 fconfig = IOS_Open("/shared2/sys/net/02/config.dat", IPC_OPEN_RW);
	if (fconfig < 0) {
		printf("Error opening config.dat: %d\n", fconfig);
		goto error;
	}
	
	ret = IOS_Read(fconfig, &nc, sizeof(netconfig_t));
	
	if (ret < 0) {
		printf("Error reading from config.dat: %d\n", ret);
		goto error;
	}
	
	for (int i = 0; i < 3; i++) {
		if (nc.connection[i].flags & 0x80) {
			//nc.connection[i].flags |= 0x04; // According to wiibrew bit 2 controls dns but that may have changed because my tests only have it work with bit 4
			nc.connection[i].dns1[0] = 8;
			nc.connection[i].dns1[1] = 8;
			nc.connection[i].dns1[2] = 8;
			nc.connection[i].dns1[3] = 8;
			nc.connection[i].dns2[0] = 8;
			nc.connection[i].dns2[1] = 8;
			nc.connection[i].dns2[2] = 4;
			nc.connection[i].dns2[3] = 4;
		}
	}
	
	IOS_Seek(fconfig, 0, 0);
	
	ret = IOS_Write(fconfig, &nc, sizeof(netconfig_t));
	if (ret < 0) {
		printf("Error writing to config.dat: %d\n", ret);
		goto error;
	}
	IOS_Close(fconfig);
	printf("OK.\n");
	printf("Resetting IOS... ");
	IOS_ReloadIOS(IOS_GetVersion());
	printf("OK\n");
#endif
	
	printf("Attempting to load boot.elf from SD...\n");
	try_sd_load();
	
	printf("Attempting to load boot.elf from network...\n");
	printf("Configuring network... ");

	// Configure the network interface
	ret = if_config (localip, netmask, gateway, TRUE, 20);
	if (ret>=0) {
		printf("OK.\n");
		printf ("network configured, ip: %s, gw: %s, mask %s\n", localip, gateway, netmask);
		http_load();
	} else {
		printf("FAIL!\n");
		printf("ERROR: if_config: %d\n", ret);
	}
	
error:
	for(;;)
		;
	
	return 0;
}

const static char http_10_200[] = "HTTP/1.0 200 OK";
const static char http_11_200[] = "HTTP/1.1 200 OK";

const static char http_get_boot[] = "GET /installer-latest.elf HTTP/1.1";
const static char http_host_header[] = "Host: ";
const static char http_useragent_header[] = "User-Agent: ";
const static char http_end_header[] = "\r\n";

const static char payload_server[] = "hbc.hackmii.com";

const static char network_loader_ua[] = "Network Loader";

struct http_header {
	char* name;
	char* value;
};
typedef struct http_header t_header;

struct http_headers {
	u32 num_headers;
	t_header headers[];
};

#define add_char(x) \
	*write_pointer++ = x; \
	new_header = realloc(current_header, write_pointer - current_header + 1); \
	if (new_header == NULL) { \
		free(current_header); \
		printf("Out of memory!\n"); \
		goto err_out; \
	} \
	write_pointer = (char*)((u32)new_header + (u32)write_pointer - (u32)current_header); \
	current_header = new_header;

void* recv_headers(int sock) {
	int last_header = 1;
	int num_headers = 0;
	struct http_headers* hdrs = malloc(sizeof(struct http_headers));
	if (hdrs == NULL) {
		printf("Out of memory!\n");
		return NULL;
	}
	
	do {
		last_header = 1;
		
		char* current_header = malloc(1);
		char* write_pointer = current_header;
		char* new_header = NULL;
		
		char* name_field = NULL;
		char* value_field = NULL;
		
		int return_recv = 0;
		int header_field_done = 0;
		int done = 0;
		char current;
		
		int bytes_read = 0;
		
		while (!done && (bytes_read = net_read(sock, &current, 1)) >= 0) {
			if (bytes_read == 0)
				continue;
			
			switch (current) {
				case 0x0d:
					if (!return_recv) {
						return_recv = 1;
						break;
					}
					
					add_char(current);
					break;
				case 0x0a:
					if (return_recv) {
						done = 1;
						add_char(0x00);
						break;
					}
					
					add_char(current);
					break;
				case ':':
					if (!header_field_done) {
						header_field_done = 1;
						add_char(0x00);
						name_field = current_header;
						current_header = malloc(1);
						write_pointer = current_header;
						break;
					} 
					// FALLTHROUGH
				default:
					last_header = 0;
					
					if (return_recv) {
						return_recv = 0;
						add_char(0x0d);
					}
					
					add_char(current);
					break;
			}
		}
		
		//Check if only \r\n was sent
		if ((u32)current_header == ((u32)write_pointer - 1)) {
			free(current_header);
		} else {
			if (name_field)
				value_field = current_header;
			else
				name_field = current_header;
			
			// We need to add it to the list
			struct http_headers* tmp_hdrs = realloc(hdrs, sizeof(struct http_headers) + ((num_headers + 1) * sizeof(t_header)));
			if (tmp_hdrs == NULL) {
				printf("Out of memory!\n");
				goto err_out;
			}
			num_headers++; // Only add one after "goto err_out" which expects num_headers to still be accurate.
			hdrs = tmp_hdrs;
			
			hdrs->headers[num_headers - 1].name = name_field;
			hdrs->headers[num_headers - 1].value = value_field;
		}
	} while (!last_header);
	
	hdrs->num_headers = num_headers;
	
	return hdrs;
	
	err_out:
	for (u32 i = 0; i < num_headers; i++) {
		free(hdrs->headers[i].name);
		free(hdrs->headers[i].value);
	}
	free(hdrs);
	return NULL;
}

void free_http_headers(struct http_headers* hdrs) {
	if (hdrs == NULL)
		return;
	for (u32 i = 0; i < hdrs->num_headers; i++) {
		free(hdrs->headers[i].name);
		free(hdrs->headers[i].value);
	}
	free(hdrs);
}

char upppercase(char c) {
	if (c >= 'a' && c <= 'z')
		return c - 0x20;
	return c;
}

// Compares two strings ignoring case as well as leading and trailing spaces.
// Returns 0 if equal and 1 if not.
int compare_headers(const char* hdr1, const char* hdr2) {
	if (hdr1 == NULL || hdr2 == NULL)
		return 1;
	
	const char* start1 = hdr1;
	const char* start2 = hdr2;
	while (*start1 == ' ')
		start1++;
	while (*start2 == ' ')
		start2++;
	
	const char* end1 = start1 + strlen(start1);
	const char* end2 = start2 + strlen(start2);
	if (end1 != start1) {
		end1--;
		while (end1 != start1 && *end1 == ' ')
			end1--;
		end1++;
	}
	if (end2 != start2) {
		end2--;
		while (end2 != start2 && *end2 == ' ')
			end2--;
		end2++;
	}
	
	if (*start1 == 0 && *start2 == 0)
		return 0;
	
	while (upppercase(*start1++) == upppercase(*start2++)) {
		if (start1 == end1 && start2 == end2)
			return 0;
	}
	
	return 1;
}

void http_load() {
	int sock;
	int ret;
	struct sockaddr_in client;
	
	printf("Attempting to resolve: %s\n", payload_server);
	
	struct hostent* dns_result;
	
	for (int i = 0; i < 3; i++) {
		dns_result = net_gethostbyname(payload_server);
		
		if (dns_result != NULL) {
			break;
		} else {
			printf("ERROR: hostent was null\n");
			if (i != 2)
				printf("Trying again: attempt %d of 3\n", i + 1);
			else
				for(;;);
		}
	}
	
	if (dns_result->h_addr_list[0] == NULL) {
		printf("ERROR: Zero items in h_addr_list\n");
		return;
	}
	
	printf("Got: %s\n", dns_result->h_name);
	for (unsigned int i = 0; dns_result->h_addr_list[i] != NULL; i++) {
		printf("\t%s\n", inet_ntoa(*(struct in_addr*)(dns_result->h_addr_list[i])));
	}
	
	struct in_addr* servaddr = (struct in_addr*)(dns_result->h_addr_list[0]);
	
	printf("Using %s\n", inet_ntoa(*servaddr));

	sock = net_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

	if (sock == INVALID_SOCKET) {
      printf ("ERROR: Cannot create a socket!\n");
    } else {

		memset(&client, 0, sizeof(client));
		
		client.sin_family = AF_INET;
		client.sin_port = htons(80);
		client.sin_addr = *servaddr;
		
		ret = net_connect(sock, (struct sockaddr *) &client, sizeof(client));
		if (ret) {
			printf("ERROR: net_connect %d connecting to server!\n", ret);
			return;
		}
		
		net_write(sock, http_get_boot, strlen(http_get_boot));
		net_write(sock, http_end_header, strlen(http_end_header));
		
		net_write(sock, http_host_header, strlen(http_host_header));
		net_write(sock, payload_server, strlen(payload_server));
		net_write(sock, http_end_header, strlen(http_end_header));
		
		net_write(sock, http_useragent_header, strlen(http_useragent_header));
		net_write(sock, network_loader_ua, strlen(network_loader_ua));
		net_write(sock, http_end_header, strlen(http_end_header));
		
		net_write(sock, http_end_header, strlen(http_end_header));
		
		struct http_headers* hdrs = recv_headers(sock);
		if (hdrs == NULL) {
			printf("ERROR: hdrs was null\n");
			net_close(sock);
			return;
		}
		
		for (u32 i = 0; i < hdrs->num_headers; i++)
			printf("Got Header: %s: %s\n", hdrs->headers[i].name, hdrs->headers[i].value);
		
		for (u32 i = 0; i < hdrs->num_headers; i++) {
			if (!compare_headers(hdrs->headers[i].name, http_11_200) || !compare_headers(hdrs->headers[i].name, http_10_200)) {
				printf("\tFound 200 response!\n"); //TODO: Check for more than just 200 to find errors
				break;
			}
			if (i + 1 == hdrs->num_headers) {
				printf("ERROR: Didn't get 200 response!\n");
				free_http_headers(hdrs);
				net_close(sock);
				return;
			}
		}
		
		u32 content_length = 0;
		const static char content_length_str[] = "Content-Length";
		
		for (u32 i = 0; i < hdrs->num_headers; i++) {
			if (!compare_headers(hdrs->headers[i].name, content_length_str)) {
				content_length = atoi(hdrs->headers[i].value);
				break;
			}
		}
		
		if (content_length == 0) {
			printf("ERROR: Either Content-Length was 0 or wasn't sent at all\n");
			free_http_headers(hdrs);
			net_close(sock);
			return;
		}
		
		printf("Detected content length of %d\n", content_length);
		
		char* payload = malloc(content_length);
		if (payload == NULL) {
			free_http_headers(hdrs);
			net_close(sock);
			printf("Out of memory!\n");
			return;
		}
		
		printf("Downloading payload... ");
		fflush(stdout);
		char* write_pointer = payload;
		while ((write_pointer - payload) != content_length) {
			ret = net_read(sock, write_pointer, 1024);
			if (ret < 0) {
				net_close(sock);
				free(payload);
				free_http_headers(hdrs);
				printf("ERROR: net_read returned %d\n", ret);
				return;
			}
			write_pointer += ret;
		}
		printf("OK\n");
		
		net_close(sock);
		
		free_http_headers(hdrs);
		
		printf("File magic: %c%c%c%c\n", payload[0], payload[1], payload[2], payload[3]);
		
		boot_elf(payload, content_length);
	}
	return;
}

void boot_elf(void *payload, size_t size) {
	printf("Shutting down IOS Subsystems... ");
	fflush(stdout);
	__IOS_ShutdownSubsystems();
	printf("OK\n");
	printf("Shutting down CPU ISR... ");
	fflush(stdout);
	u32 level;
	_CPU_ISR_Disable(level);
	printf("OK\n");
	printf("Shutting down exception vectors... ");
	fflush(stdout);
	__exception_closeall();
	printf("OK\n");
	
	printf("Copying ELF loading stub... ");
	fflush(stdout);
	memcpy((void*)0x81330000, loader_bin, loader_bin_len);
	sync_before_exec((void*)0x81330000, loader_bin_len);
	printf("OK\n");
	printf("Jumping to entry point!\n");
	fflush(stdout);
	
	void (*entry)(void*, u32) = (void*)0x81330000;
	entry(payload, size);
	
	printf("If you can see this then something has gone very wrong!\n");
	fflush(stdout);
	for(;;)
		;
}
