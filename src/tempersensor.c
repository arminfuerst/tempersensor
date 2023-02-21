/*
 * tempersensor is a program to supply the temperature values
 * from TEMPer USB devices from RDing (www.pcsensor.com) in MRTG style.
 * Additional infos (including a license notice) are at the end of this file.
 */

// https://github.com/urwen/temper - probably additional infos...

#include <ctype.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <linux/hidraw.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include "mrtg.h"

#define PROGRAMNAME "tempersensor"
#define VERSION "0.1.8"
#define USBCommunicationTimeout 5000

/*
 * some USB definitions
 *
 * https://www.codeproject.com/Tips/1007522/Temper-USB-Thermometer-in-Csharp
 */

#define USB_request_type 0x21
#define USB_set_report 0x09
#define USB_output_report 0x0200 // add report to this
#define USB_report0 0x00
#define USB_report1 0x01

/*
 * Temper magic strings
 *
 */

//const static unsigned char query_vals[] = { 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
const static unsigned char query_vals[] = { 0x01, 0x80, 0x33, 0x01, 0x00, 0x00, 0x00, 0x00 };
const static unsigned char query_firmware[] = { 0x01, 0x86, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00 };
#define ENDPOINT_INTERRUPT_IN 0x82 // comment from pcsensor.c: endpoint 0x81 address for IN
#define ENDPOINT_INTERRUPT_OUT 0x02 // comment from pcsensor.c: endpoint 1 address for OUT
#define ENDPOINT_BULK_IN 0x82 // comment from pcsensor.c: endpoint 0x81 address for IN
#define ENDPOINT_BULK_OUT 0x00 // comment from pcsensor.c: endpoint 1 address for OUT
#define INTERFACE1 0x00
#define INTERFACE2 0x01
#define ANSWERSIZE 8

/*
 * list of vendor IDs and product IDs being supported
 * the order of the two arrays is relevant, only the
 * nth entry of both arrays in combination define a
 * supported device.
 */
const static unsigned short vendorId[] =
{
	0x1130, // Tenx Technology, Inc. Foot Pedal/Thermometer (untested)
	0x0c45, // TEMPer / TEMPer1F_V1.3r1F (tested), 
		// TEMPer / TEMPerF1.4 (some tests)
	0x413d, // TEMPer / TEMPerGold_V3.1 (untested),
		// TEMPerHUM / TEMPerX_V3.1 (untested),
		// TEMPerHUM / TEMPerX_V3.3 (tested),
		// TEMPer2 / TEMPerX_V3.3 (untested)
		// TEMPer1F / TEMPerX_V3.3 (untested)
	0x1a86,  // TEMPerX232 / TEMPerX232_V2.0 (untested)
	0x1a86  // TEMPerHUM / TEMPerHUM_V3.9
};
const static unsigned short productId[] =
{
	0x660c,
	0x7401,
	0x2107,
	0x5523,
	0xe025
};

struct config
{
	int debug;
	int precision;
	bool fahrenheit;
	int in_sensor; /* which sensor to report as "IN" */
	int out_sensor; /* which sensor to report as "OUT" */
	float calibration_in;
	float calibration_out;
};

/* where in the values-array to find which sensor */
#define NO_SENSOR -1
#define INT_TEMP 0
#define INT_HUM 1
#define EXT_TEMP 2
#define EXT_HUM 3

struct device
{
	char firmware[17];
	uint16_t vendor_id;
	uint16_t product_id;
	char *hidraw_devpath;
	int fd;
	int amount_value_responses; /* amount of bytes expected to value-request */
	int conversion_method; /* devices have different types of representation */
	float values[4]; /* the values read from the device */
	int sensors[2][2]; /* define which part of the response defines which sensor */
};

/*
 * global vars
 */

struct config config;
struct device device;

/*
 * forward declarations
 */

void test_calc();

void printVersion()
{
	printf("%s version %s, libmrtg version %s\n", PROGRAMNAME, VERSION, libmrtg_version());
}

/*
 * option_string
 *
 * generate the string with options for getopt_long based
 * on the struct so additional parameters don't have to be
 * added on two places.
 */

char *option_string(const struct option *long_options)
{
	int i;
	int len;
	char *returnstring;
	char tmp[2];

	returnstring = (char *) malloc(1);
	returnstring[0] = 0;
	if (long_options == NULL)
		return returnstring;
	for (i = 0; long_options[i].name; i++)
	{
		if (((long_options[i].val >= 65) &&
			(long_options[i].val <= 90)) ||
			((long_options[i].val >= 97) &&
			(long_options[i].val <= 122)))
		{
			len = strlen(returnstring);
			if (long_options[i].has_arg)
			{
				returnstring = (char *) realloc(returnstring, len + 2 + 1);
			}
			else
			{
				returnstring = (char *) realloc(returnstring, len + 1 + 1);
			}
			(void)sprintf(tmp,"%c", long_options[i].val);
			strcat(returnstring, tmp);
			if (long_options[i].has_arg)
			{
				strcat(returnstring,":");
			}
		}
	}
	return returnstring;
}

void usage()
{
	printVersion();
	printf("\t--calibration-in=[-]n.n\tmodify result for IN\n");
	printf("\t--calibration-out=[-]n.n\tmodify result for OUT\n");
	printf("\t--conversion-method=METHOD\toverride conversion from response\n");
	printf("\t\t\t\t\tvalues for METHOD:\n");
	printf("\t\t\t\t\t 1 = two's complement with 4 bits used\n");
	printf("\t\t\t\t\t     for fraction\n");
	printf("\t\t\t\t\t 2 = two's complement with 16 bits,\n");
	printf("\t\t\t\t\t     value assumed multiplied by 100\n");
	printf("\t-d, --debug\t\t\tshow debug output\n");
	printf("\t-f, --fahrenheit\t\treport temperatures in Fahrenheit\n");
	printf("\t-h, --help\t\t\thelp\n");
	printf("\t-p, --precision=LEN\t\tamount of decimal places (default=0)\n");
	printf("\t--report-in=SENSOR\t\treport sensor SENSOR as IN value\n");
	printf("\t--report-out=SENSOR\t\treport sensor SENSOR as OUT value\n");
	printf("\t\t\t\t\tvalues for SENSOR:\n");
	printf("\t\t\t\t\t it = internal temperature\n");
	printf("\t\t\t\t\t et = external temperature\n");
	printf("\t\t\t\t\t ih = internal humitity\n");
	printf("\t\t\t\t\t eh = external humitity\n");
	printf("\t-t, --test\t\t\trun tests for temperature calculation\n");
	printf("\t-V, --version\t\t\tdisplay version information\n");
}

void parse_parameters(int argc, char **argv)
{
	int c;
	int option_index;
	char *os;

	/* set default values */
	config.debug = 0;
	config.precision = 0;
	config.fahrenheit = false;
	config.in_sensor = -1;
	config.out_sensor = -1;
	config.calibration_in = 0.0;
	config.calibration_out = 0.0;
	device.conversion_method = -1;
	device.fd = -1;

	/* create structure of options */
	static struct option temper_options[] =
	{
		{"calibration-in", required_argument, 0, 0},
		{"calibration-out", required_argument, 0, 1},
		{"conversion-method", required_argument, 0, 2},
		{"debug", no_argument, 0, 'd'},
		{"fahrenheit", no_argument, 0, 'f'},
		{"help", no_argument, 0, 'h'},
		{"precision", required_argument, 0, 'p'},
		{"report-in", required_argument, 0, 3},
		{"report-out", required_argument, 0, 4},
		{"test", no_argument, 0, 't'},
		{"version", no_argument, 0, 'V'},

	};
	os = option_string(temper_options);

	while ((c = getopt_long (argc, argv, os, temper_options, &option_index)) != -1)
	{
		switch (c)
		{
			float ftmp;
			int itmp;
			case 0: // calibration-in
			case 1: // calibration-out
				if (strchr(optarg, ','))
				{
					fprintf(stderr, "Error: '%s' must not contain ','.\n", optarg);
					free(os);
					exit(EXIT_FAILURE);
				}
				else if (!(sscanf(optarg, "%f", &ftmp) == 1))
				{
					fprintf(stderr, "Error: '%s' is not a float.\n", optarg);
					free(os);
					exit(EXIT_FAILURE);
				}
				if (c == 0)
				{
					config.calibration_in = ftmp;
				}
				else
				{
					config.calibration_out = ftmp;
				}
				break;
			case 2: // conversion-method
				if (!(sscanf(optarg, "%i", &device.conversion_method) == 1))
				{
					fprintf(stderr, "Error: '%s' is not numeric.\n", optarg);
					free(os);
					exit(EXIT_FAILURE);
				}
				if ((device.conversion_method < 1) ||
					(device.conversion_method > 2))
				{
					fprintf(stderr, "Invalid value for conversion-method: '%s'\n", optarg);
					free(os);
					exit(EXIT_FAILURE);
				}
				break;
			case 3: // report-in
			case 4: // report-out
				if (!strcmp(optarg, "it"))
					itmp = INT_TEMP;
				else if (!strcmp(optarg, "et"))
					itmp = EXT_TEMP;
				else if (!strcmp(optarg, "ih"))
					itmp = INT_HUM;
				else if (!strcmp(optarg, "eh"))
					itmp = EXT_HUM;
				else
				{
					fprintf(stderr, "Invalid value '%s' for option '%s'\n",
						optarg, temper_options[option_index].name);
					usage();
					free(os);
					exit(EXIT_FAILURE);
				}
				if (c == 3)
					config.in_sensor = itmp;
				else
					config.out_sensor = itmp;
				break;
			case 'd':
				config.debug = 1;
				break;
			case 'f':
				config.fahrenheit = true;
				break;
			case '?':
			case 'h':
				usage();
				free(os);
				exit(EXIT_SUCCESS);
				break;
			case 'p':
				if (!(sscanf(optarg, "%i", &config.precision) == 1))
				{
					fprintf(stderr, "Error: '%s' is not numeric.\n", optarg);
					free(os);
					exit(EXIT_FAILURE);
				}
				break;
			case 't':
				test_calc();
				free(os);
				exit(EXIT_SUCCESS);
				break;
			case 'V':
				printVersion();
				free(os);
				exit(EXIT_SUCCESS);
				break;
			default:
				if (isprint(optopt))
				{
					fprintf(stderr, "Unknown option `-%c'.\n", optopt);
				}
				else
				{
					fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
				}
				free(os);
				exit(EXIT_FAILURE);
		}
	}
	free(os);
}

/* 
 * debug_print
 */

void debug_print(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	
	if (config.debug > 0)
	{
		vfprintf(stderr, format, args);
	}

	va_end(args);
}

float fahrenheit(float celsius)
{
	return ((celsius * (9.0 / 5.0)) + 32.0);
}

/*
 * extend_errormessage
 *
 * extends the given errormessage with the textual representation
 * of the errno error
 */

char *extend_errormessage(const char* errormessage, int errnum)
{
	char *fullmsg;
	
	fullmsg = (char *) malloc(strlen(errormessage) +
		strlen(strerror(errnum)) + 3);
	sprintf(fullmsg, "%s: %s", errormessage,
		strerror(errnum));
	
	return fullmsg;
}


/*
 * print_error
 *
 * simplify returning errors by just having to specify the errormessage
 */

void print_error(const char *errormessage)
{
	debug_print("%s\n", errormessage);
	print_mrtg_error(PROGRAMNAME, VERSION, errormessage);
}

/*
 * print_values
 *
 * simplify printing values by just having to specify the values
 */
void print_values(float in, float out, int precision)
{
	char instr[30];
	char outstr[30];
	char fmtstr[10];

	(void)sprintf(fmtstr, "%%.%if", precision);
	if (config.fahrenheit)
	{
		in = fahrenheit(in);
		out = fahrenheit(out);
	}
	if (in > -999.0)
	{
		in += config.calibration_in;
		(void)sprintf(instr, fmtstr, in);
	}
	else
		(void)sprintf(instr, INVALID_VALUE);
	if (out > -999.0)
	{
		out += config.calibration_out;
		(void)sprintf(outstr, fmtstr, out);
	}
	else
		(void)sprintf(outstr, INVALID_VALUE);
	print_mrtg_values(PROGRAMNAME, VERSION, instr, outstr);
}


/* 
 * debug_print_byte
 */
void debug_print_byte(const unsigned char *string, size_t ssize, const char *format, ...)
{
	int c = 0;
	va_list args;
	va_start(args, format);

	if (config.debug <= 0)
	{
		return;
	}
	while (c<ssize)
	{
		fprintf(stderr, "%02x ", string[c] & 0xFF);
		c++;
	}
	fprintf(stderr, "(");
	vfprintf(stderr, format, args);
	fprintf(stderr, ")\n");

	va_end(args);
}

/*
 * is_device_supported
 *
 * returns true if the given device is in the list of
 * supported devices, otherwise returns false
 */
bool is_device_supported(uint16_t idVendor, uint16_t idProduct)
{
	int known_devices;
	int cnt = 0;

	// calculate how many devices are in the list of supported devices
	known_devices = sizeof(vendorId) / sizeof(vendorId[0]);

	while (cnt < known_devices)
	{
		if ((idVendor == vendorId[cnt])
			&& (idProduct == productId[cnt]))
		{
			debug_print("Device %04x:%04x found\n", vendorId[cnt], productId[cnt]);
			return true;
		}
		cnt++;
	}
	return false;
}


/*
 * cleanup
 *
 * close and exit everything
 */

void cleanup()
{
	if (device.hidraw_devpath != NULL)
	{
		free(device.hidraw_devpath);
	}
	if (device.fd >= 0)
	{
		close(device.fd);
	}
}

char *send_command(const char *cmdname, const unsigned char *question, size_t qsize)
{
	int r;
	char errmsg[45];
	char *fullerr = NULL;

	debug_print_byte(question, qsize, "command '%s' sent", cmdname);
	r = write(device.fd, question, qsize);
	if (r < 0)
	{
		sprintf(errmsg, "Error sending command '%s'", cmdname);
		fullerr = extend_errormessage(errmsg, errno);
		return fullerr;
	}

	return fullerr;
}

int read_timeout(int fd, void *buf, size_t count)
{
	fd_set set;
	struct timeval timeout;
	int rv;
	int r;

	FD_ZERO(&set);
	FD_SET(fd, &set);

	timeout.tv_sec = 0;
	/* set a timeout of 1 second */
	timeout.tv_usec = 1*1000*1000;

	rv = select(fd + 1, &set, NULL, NULL, &timeout);
	if (rv == -1)
		return -1;
	else if (rv == 0)
		return -2;

	r = read(fd, buf, count);
	if (r < 0)
		return -1;

	return r;
}

char *read_answer_cond(const char *cmdname, unsigned char *answer, bool ignore_readerror)
{
	int r;
	char *errmsg = NULL;
	char *fullerr = NULL;

	r = read_timeout(device.fd, answer, ANSWERSIZE);
	if (r < 0)
	{
		if (! ignore_readerror)
		{
			errmsg = (char *) malloc (29 + strlen(cmdname));
			sprintf(errmsg, "Error reading response to '%s'", cmdname);
			if (r == -2)
			{
				fullerr = malloc (strlen(errmsg) + 10);
				fullerr[0] = 0;
				strcat(fullerr, errmsg);
				strcat(fullerr, ": Timeout");
			}
			else
			{
				fullerr = extend_errormessage(errmsg, errno);
			}
			free(errmsg);
		}
		return fullerr;
	}
	debug_print_byte(answer, ANSWERSIZE, "response to '%s'", cmdname);

	return fullerr;
}

char *read_answer(const char *cmdname, unsigned char *answer)
{
	return read_answer_cond(cmdname, answer, false);
}


int get_firmware_string()
{
	char *errmsg = NULL;
	unsigned char answer[9];
	int cnt = 0;

	device.firmware[0] = 0;
	errmsg = send_command("query firmware", query_firmware, sizeof(query_firmware));
	if (errmsg != NULL) 
	{
		print_error(errmsg);
		free(errmsg);
		return 0;
	}
	while (cnt < 2)
	{
		errmsg = read_answer("query firmware", answer);
		if (errmsg != NULL)
		{
			print_error(errmsg);
			free(errmsg);
			return 0;
		}
		strncat(device.firmware, (char *)answer, 8);
		cnt++;
	}

	return 1;
}

/*
 * open device and query firmware to deduce device capabilities
 *
 */

int init_device()
{
	char *errmsg;

	device.fd = open(device.hidraw_devpath, O_RDWR);
	if (device.fd < 0)
	{
		errmsg = extend_errormessage("Error opening device", errno);
		print_error(errmsg);
		free(errmsg);
		return 0;
	}

	if (!get_firmware_string())
	{
		return 0;
	}
	
	return 1;
}

/*
 * evaluate_device_details
 *
 * based of the details collected about the device, define
 * how to interact with the device
 */

int evaluate_device_details()
{
	int cnt = 0;
	/*
	 * The software is only tested with TEMPer1F_V1.3, TEMPerF1.4
	 * and TEMPerHUM reporting as 'TEMPerX_V3.3'. Configuration of
	 * all other devices is collected from several forums and from
	 * https://github.com/urwen/temper
	 * Idea:
	 * - Probably provide a way to override detection / configure
	 *   device through parameters
	 */

	/*
	 * Since not all devices support all sensors, assume all
	 * possible sensors to return invalid values, existing
	 * sensors will overwrite with real values later.
	 */
	while (cnt < 4)
	{
		device.values[cnt] = -999.0;
		cnt++;
	}
	if ((device.vendor_id == 0x0c45) &&
		(device.product_id == 0x7401))
	{
		if (!strncmp(device.firmware, "TEMPer1F_V1.3r1F", 13))
		{
			debug_print("Detected TEMPer1F_V1.3\n");
			device.amount_value_responses = 1;
			if (device.conversion_method == -1)
				device.conversion_method = 1;
			device.sensors[0][0] = NO_SENSOR;
			device.sensors[0][1] = EXT_TEMP;
			if (config.in_sensor == -1)
				config.in_sensor = EXT_TEMP;
			if (config.out_sensor == -1)
				config.out_sensor = EXT_TEMP;
		}
		else if (!strncmp(device.firmware, "TEMPerF1.4", 10))
		{
			debug_print("Detected TEMPer1F1.4\n");
			device.amount_value_responses = 1;
			if (device.conversion_method == -1)
				device.conversion_method = 1;
			device.sensors[0][0] = INT_TEMP;
			device.sensors[0][1] = NO_SENSOR;
			if (config.in_sensor == -1)
				config.in_sensor = INT_TEMP;
			if (config.out_sensor == -1)
				config.out_sensor = INT_TEMP;
		}
		else
		{
			debug_print("Unknown firmware '%s'\n", device.firmware);
			print_error("Unknown 0c45:7401 device");
			return 0;
		}
	}
	else if ((device.vendor_id == 0x1130) &&
		(device.product_id == 0x660c))
	{
		/* 
		 * TODO: learn details about this device
		 *  - does it reply properly to the firmware?
		 *  - which firmware-replies are known
		 *  - from some postings the sensors are assumed to be 
		 *    identical to "TEMPer1F_V1.3", but there is no confirmation
		 */
		debug_print("Detected Tenx Technology, Inc. Foot Pedal/Thermometer (untested)\n");
		device.amount_value_responses = 1;
		if (device.conversion_method == -1)
			device.conversion_method = 1;
		device.sensors[0][0] = NO_SENSOR;
		device.sensors[0][1] = INT_TEMP;
		if (config.in_sensor == -1)
			config.in_sensor = INT_TEMP;
		if (config.out_sensor == -1)
			config.out_sensor = INT_TEMP;
	}
	else if ((device.vendor_id == 0x1a86) &&
		(device.product_id == 0x5523))
	{
		/*
		 * TODO: https://github.com/urwen/temper has a description
		 * how these devices can be interacted with - they seem to be
		 * very different, probably they are too different to be
		 * implemented?
		 */
		print_error("TEMPerX232 / TEMPerX232_V2.0 (1a86:5523) detected - unsupported yet\n");
		return 0;
	}
	else if ((device.vendor_id == 0x413d) &&
		(device.product_id == 0x2107))
	{
		/*
		 * configuration based on the implementation of
		 * https://github.com/urwen/temper/blob/master/temper.py
		 */
		if (!strncmp(device.firmware, "TEMPerGold_V3.1", 15))
		{
			debug_print("Detected TEMPerGold_V3.1 (untested!)\n");
			device.amount_value_responses = 1;
			if (device.conversion_method == -1)
				device.conversion_method = 2;
			device.sensors[0][0] = INT_TEMP;
			device.sensors[0][1] = NO_SENSOR;
			if (config.in_sensor == -1)
				config.in_sensor = INT_TEMP;
			if (config.out_sensor == -1)
				config.out_sensor = INT_TEMP;
		}
		else if (!strncmp(device.firmware, "TEMPerX_V3.1", 12))
		{
			debug_print("Detected TEMPerX_V3.1 (untested!)\n");
			device.amount_value_responses = 2;
			if (device.conversion_method == -1)
				device.conversion_method = 2;
			device.sensors[0][0] = INT_TEMP;
			device.sensors[0][1] = INT_HUM;
			device.sensors[1][0] = EXT_TEMP;
			device.sensors[1][1] = EXT_HUM;
			if (config.in_sensor == -1)
				config.in_sensor = INT_HUM;
			if (config.out_sensor == -1)
				config.out_sensor = INT_TEMP;
		}
		else if (!strncmp(device.firmware, "TEMPerX_V3.3", 12))
		{
			debug_print("Detected TEMPerX_V3.3\n");
			device.amount_value_responses = 1;
			if (device.conversion_method == -1)
				device.conversion_method = 2;
			device.sensors[0][0] = INT_TEMP;
			device.sensors[0][1] = INT_HUM;
			device.sensors[1][0] = NO_SENSOR;
			device.sensors[1][1] = NO_SENSOR;
			if (config.in_sensor == -1)
				config.in_sensor = INT_HUM;
			if (config.out_sensor == -1)
				config.out_sensor = INT_TEMP;
		}
		else
		{
			debug_print("Unknown firmware '%s'\n", device.firmware);
			print_error("Unknown 413d:2107 device");
			return 0;
		}
	}
	else if ((device.vendor_id == 0x1a86) &&
		(device.product_id == 0xe025))
	{
		if (!strncmp(device.firmware, "TEMPerHUM_V3.9", 14))
		{
			debug_print("Detected TEMPerHUM_V3.9\n");
			device.amount_value_responses = 1;
			if (device.conversion_method == -1)
				device.conversion_method = 2;
			device.sensors[0][0] = INT_TEMP;
			device.sensors[0][1] = INT_HUM;
			device.sensors[1][0] = NO_SENSOR;
			device.sensors[1][1] = NO_SENSOR;
			if (config.in_sensor == -1)
				config.in_sensor = INT_HUM;
			if (config.out_sensor == -1)
				config.out_sensor = INT_TEMP;
		}
		else
		{
			debug_print("Unknown firmware '%s'\n", device.firmware);
			print_error("Unknown 1a86:e025 device");
			return 0;
		}
	}
	else
	{
		debug_print("Unknown firmware '%s'\n", device.firmware);
		print_error("Unknown device\n");
		return 0;
	}
	return 1;
}

/*
 * calc_value
 *
 * calculates value from response from given starting character
 */

float calc_value(const unsigned char *valuestring, int startchar)
{
	/*
	 * To be improved: send only a string with two characters
	 * and make selection of the correct characters outside this
	 * function, combine it with autodetection based on Firmware string.
	 */

	/*
	   reverse engineering results for TEMPer 1.3:
	    + relevant for this device are positions 4 + 5
	    + in good case, the lower sigificant part of position 5
	      is always 0
	    + if error case, the lower sigificant part of position 5
	      is F (or not zero?)
	    + the value is represented in two's complement
	    + position 4 is the part before the comma
	    + the higher 4 bits of position 5 are after the comma
	 */

	if (device.conversion_method == 1)
	{
		/*
		 * conversion_method 1: value in two's complement with fraction
		 * If MSB is set, the temperature is negative in 2'complement form.
		 * Since we cannot know how negative numbers are on the system,
		 * we calculate it manually. Since TEMPer only has 4 bits after the
		 * comma and they are the higher part of the byte, we shift the two
		 * bytes after concatinating 4 bits to the right, deal with 12 bits
		 * (0xFFF) and divide by 16.
		 * Alternatively, we could omit the shift to the right, deal with
		 * 16 bits (0xFFFF) and divide by 256.
		 */

		/* see whether we have valid result */
		if ((valuestring[startchar + 1] & 0x0F) != 0)
		{
			debug_print("invalid result received\n");
			return -999.0;
		}

		if ((valuestring[startchar] & 0x80) != 0)
		{
			// convert fixed comma from 2'complement form, < 0
			// 1. ignore comma
			// 2. subtract 1
			// 3. invert bits
			// 4. multiply by -1
			// 5. divide by amount of distinct values behind the comma
			//    - here we calculate with 4 bits => 2 by power of 4

			return (float)((((((valuestring[startchar] << 8) + 
				valuestring[startchar + 1]) >> 4) - 1) ^ 0xFFF) * -1) / pow(2, 4);
		}
		else
		{
			// convert fixed comma from 2'complement form, >= 0
			// 1. ignore comma
			// 2. divide by amount of distinct values behind the comma
			//    - here we calculate with 4 bits => 2 by power of 4

			return (float)(((valuestring[startchar] << 8) + valuestring[startchar + 1]) >> 4) / pow(2, 4);

		}
	}
	else if (device.conversion_method == 2)
	{
		/*
		 * conversion_method 2: value in two's complement multiplied
		 * by 100, e.g. 22.06 °C = 2206
		 */
		if ((valuestring[startchar] & 0x80) != 0)
		{
			// convert two's complement, < 0
			// 1. convert two bytes to integer
			// 2. subtract 1 from integer
			// 3. invert bits
			// 4. make negative
			// 5. after conversion, divide by 100
			return (float)(((((valuestring[startchar] << 8) + valuestring[startchar + 1])
				- 1) ^ 0xFFFF) * -1) / 100.0;
		}
		else
		{
			// convert two's complement, >= 0
			// 1. convert to bytes to integer
			// 2. after conversion, divide by 100
			return (float)((valuestring[startchar] << 8) + valuestring[startchar + 1]) / 100.0;
		}
	}
	else
	{
		/*
		 * if an unknown conversion_method is defined,
		 * return invalid value
		 */
		return -999.0;
	}
}


/*
 * read values from temper
 *
 */
int query_values()
{
	#define RETRIES 10
	char *errmsg = NULL;
	unsigned char answer[9];
	int response;
	int sensor;
	int cnt = 0;
	int received_responses;

	while (cnt < RETRIES)
	{
		response = 0;
		received_responses = 0;
		errmsg = send_command("query values", query_vals, sizeof(query_vals));
		if (errmsg != NULL) 
		{
			if ((cnt + 1) < RETRIES)
			{
				debug_print("%s, retry %i/%i\n", errmsg, cnt + 1, RETRIES);
				free(errmsg);
			}
			else
			{
				print_error(errmsg);
				free(errmsg);
				return 0;
			}
		}
		while (response < device.amount_value_responses)
		{
			errmsg = read_answer("query values", answer);
			if (errmsg != NULL)
			{
				debug_print("%s on try %i/%i\n", errmsg, cnt + 1, RETRIES);
				if ((cnt + 1) >= RETRIES)
				{
					print_error(errmsg);
					free(errmsg);
					return 0;
				}
				free(errmsg);
			}
			else
			{
				// per response there are up to 2 sensors
				sensor = 0;
				while (sensor < 2)
				{
					if (device.sensors[response][sensor] >= 0)
					{
						device.values[device.sensors[response][sensor]] = 
							calc_value(answer, (2 + (sensor * 2)));
					}
					sensor++;
				}
				received_responses++;
			}

			response++;
		}
		if (received_responses >= device.amount_value_responses)
		{
			break;
		}
		cnt++;
	}
	/* 
	 * TODO: this code is left from an experimental version
	 * where only one value was supported.
	 * probably there is a way to report this when at least one
	 * sensor returns an error?
	 */

	/*
	if (*tempC == -999.0)
	{
		print_error("Error received - external sensor disconnected?");
		return 0;
	}
	*/

	return 1;
}

int char_index(const char *string, char c)
{
	for (int i = 0; string[i] != '\0'; i++)
		if (string[i] == c)
			return i;

	return -1;
}

/*
 * get_devnode
 *
 * uses /sys-dirstructure to find the appropriate hidraw device
 */

int get_devnode()
{
	struct hidraw_device
	{
		uint16_t idVendor;
		uint16_t idProduct;
		char devname[261];
	};

	struct hidraw_device *devlist;
	int amount = 0;

	int add_hiddev(const char *base_path)
	{
		int fd;
		int r;
		char *filepath;
		char buf[16];
		struct hidraw_device dev;

		//dev = malloc(sizeof(dev));

		/* get Vendor ID */
		filepath = malloc(strlen(base_path) + 19);
		(void)sprintf(filepath, "%s/../../../idVendor", base_path);
		fd = open(filepath, O_RDONLY);
		r = read(fd, buf, 16);
		if (r < 0)
		{
			print_error("Error reading idVendor");
			close(fd);
			free(filepath);
			return 0;
		}
		dev.idVendor = strtol(buf, NULL,16);
		close(fd);
		free(filepath);

		/* get Product ID */
		filepath = malloc(strlen(base_path) + 20);
		(void)sprintf(filepath, "%s/../../../idProduct", base_path);
		fd = open(filepath, O_RDONLY);
		r = read(fd, buf, 16);
		if (r < 0)
		{
			print_error("Error reading idProduct");
			close(fd);
			free(filepath);
			return 0;
		}
		dev.idProduct = strtol(buf, NULL,16);
		close(fd);
		free(filepath);

		/* get device name */
		DIR *dir = opendir(base_path);
		struct dirent *dp;

		while ((dp = readdir(dir)) != NULL)
		{
			if (strstr(dp->d_name, "hidraw"))
			{
				(void)sprintf(dev.devname, "/dev/%s", dp->d_name);
			}
		}

		/* copy the results to the array of structs */
		devlist = realloc(devlist, sizeof(dev) * (amount + 1));
		devlist[amount].idVendor = dev.idVendor;
		devlist[amount].idProduct = dev.idProduct;
		strcpy(devlist[amount].devname, dev.devname);
		debug_print("VendorId: %04x / ", devlist[amount].idVendor);
		debug_print("ProductId: %04x / ", devlist[amount].idProduct);
		debug_print("Device: '%s'\n", devlist[amount].devname);
		closedir(dir);
		amount++;
		return 1;
	}

	int find_hidraw(const char *base_path)
	{
		char *path;
		struct dirent *dp;
		DIR *dir = opendir(base_path);

		if (!dir)
			return 1;

		while ((dp = readdir(dir)) != NULL)
		{
			if ((strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0)
				&& (dp->d_type != DT_LNK))
			{
				path = malloc(strlen(base_path) + strlen(dp->d_name) + 2);
				strcpy(path, base_path);
				strcat(path, "/");
				strcat(path, dp->d_name);
				if (!strcmp(dp->d_name, "hidraw"))
				{
					if (!add_hiddev(path))
					{
						closedir(dir);
						free(path);
						return 0;
					}
				}
				else
					find_hidraw(path);
				free(path);
			}
		}

		closedir(dir);
		return 1;
	}

	device.hidraw_devpath = NULL;
	devlist = NULL;
	debug_print("Scanning for hidraw devices\n");
	if (!find_hidraw("/sys/devices"))
	{
		return 0;
	}

	device.vendor_id = 0;
	debug_print("looking for supported devices\n");
	int cnt = 0;
	while (cnt < amount)
	{
		if (is_device_supported(devlist[cnt].idVendor, devlist[cnt].idProduct))
		{
			if (device.vendor_id == 0)
			{
				device.vendor_id = devlist[cnt].idVendor;
				device.product_id = devlist[cnt].idProduct;
				device.hidraw_devpath = malloc(strlen(devlist[cnt].devname) + 1);
				strcpy(device.hidraw_devpath, devlist[cnt].devname);
				debug_print("storing %s as devpath\n", device.hidraw_devpath);
			}
			else if (device.vendor_id == devlist[cnt].idVendor)
			{
				if (strcmp(device.hidraw_devpath, devlist[cnt].devname) < 0)
				{
					debug_print("Switching devpath from %s to %s\n",
						device.hidraw_devpath, devlist[cnt].devname);
					device.hidraw_devpath = realloc(device.hidraw_devpath, strlen(devlist[cnt].devname) + 1);
					memcpy(device.hidraw_devpath, devlist[cnt].devname, strlen(devlist[cnt].devname));
				}
				else
				{
					debug_print("Not switching devpath from %s to %s\n",
						device.hidraw_devpath, devlist[cnt].devname);
				}
			}
			else
			{
				debug_print("Found two supported devices, sticking to %04x:%04x and ignoring %04x:%04x\n",
					device.vendor_id, device.product_id, devlist[cnt].idVendor, devlist[cnt].idProduct);
			}
		}
		cnt++;
	}
	free(devlist);

	/* if there still is no real device here, we didn't find any */
	if (device.vendor_id == 0)
	{
		print_error("No supported device found");
		return 0;
	}


	debug_print("Will use '%s'\n",device.hidraw_devpath);
	return 1;
}

void test_calc()
{
	unsigned char answer[4096];
	float tmp;

	// there will only be an output in debug mode
	config.debug = 1;
	device.conversion_method = 1;

	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00 }, 8);
	tmp = calc_value(answer, 4);
	debug_print("temp: %.4f / expected: error (-999.00)\n", tmp);

	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00 }, 8);
	tmp = calc_value(answer, 2);
	debug_print("temp: %.4f / expected: error (-999.00)\n", tmp);

	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0x1a, 0x1a, 0x1a, 0x10, 0x00, 0x00 }, 8);
	tmp = calc_value(answer, 4);
	debug_print("temp: %.4f / expected: 20.0625\n", tmp);

	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0x14, 0x14, 0x14, 0xd0, 0x00, 0x00 }, 8);
	tmp = calc_value(answer, 4);
	debug_print("temp: %.4f / expected: 20.8125\n", tmp);

	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0x01, 0x01, 0x01, 0x60, 0x00, 0x00 }, 8);
	tmp = calc_value(answer, 4);
	debug_print("temp: %.4f / expected: 1.3750\n", tmp);

	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00 }, 8);
	tmp = calc_value(answer, 4);
	debug_print("temp: %.4f / expected: 0.3750\n", tmp);

	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00 }, 8);
	tmp = calc_value(answer, 4);
	debug_print("temp: %.4f / expected: 0.0625\n", tmp);

	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 8);
	tmp = calc_value(answer, 4);
	debug_print("temp: %.4f / expected: 0.0000\n", tmp);

	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0x00, 0x00, 0xff, 0xf0, 0x00, 0x00 }, 8);
	tmp = calc_value(answer, 4);
	debug_print("temp: %.4f / expected: -0.0625\n", tmp);

	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0xff, 0xff, 0xff, 0x40, 0x00, 0x00 }, 8);
	tmp = calc_value(answer, 4);
	debug_print("temp: %.4f / expected: -0.7500\n", tmp);

	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00 }, 8);
	tmp = calc_value(answer, 4);
	debug_print("temp: %.4f / expected: -1.0000\n", tmp);

	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0xfe, 0xfe, 0xfe, 0xf0, 0x00, 0x00 }, 8);
	tmp = calc_value(answer, 4);
	debug_print("temp: %.4f / expected: -1.0625\n", tmp);

	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0xfe, 0xfe, 0xfe, 0x00, 0x00, 0x00 }, 8);
	tmp = calc_value(answer, 4);
	debug_print("temp: %.4f / expected: -2.0000\n", tmp);

	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0xfd, 0xfd, 0xfd, 0xf0, 0x00, 0x00 }, 8);
	tmp = calc_value(answer, 4);
	debug_print("temp: %.4f / expected: -2.0625\n", tmp);

	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0xfe, 0xfe, 0x01, 0x00, 0x00, 0x00 }, 8);
	tmp = calc_value(answer, 4);
	debug_print("temp: %.4f / expected: 1.0000\n", tmp);

	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0xfe, 0xfe, 0x00, 0x00, 0x00, 0x00 }, 8);
	tmp = calc_value(answer, 4);
	debug_print("temp: %.4f / expected: 0.0000\n", tmp);

	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0x00, 0x00, 0xff, 0xf0, 0x00, 0x00 }, 8);
	tmp = calc_value(answer, 4);
	debug_print("temp: %.4f / expected: -0.0625\n", tmp);

	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0xfe, 0xfe, 0xFF, 0x00, 0x00, 0x00 }, 8);
	tmp = calc_value(answer, 4);
	debug_print("temp: %.4f / expected: -1.0000\n", tmp);

	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0xfe, 0xfe, 0xFE, 0x00, 0x00, 0x00 }, 8);
	tmp = calc_value(answer, 4);
	debug_print("temp: %.4f / expected: -2.0000\n", tmp);

	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0xfe, 0xfe, 0xFD, 0x00, 0x00, 0x00 }, 8);
	tmp = calc_value(answer, 4);
	debug_print("temp: %.4f / expected: -3.0000\n", tmp);

	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0xee, 0xee, 0xee, 0x40, 0x00, 0x00 }, 8);
	tmp = fahrenheit(calc_value(answer, 4));
	debug_print("temp: %.4f / expected: 0.0500\n", tmp);

	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0xee, 0xee, 0xee, 0x30, 0x00, 0x00 }, 8);
	tmp = fahrenheit(calc_value(answer, 4));
	debug_print("temp: %.4f / expected: -0.0625\n", tmp);

	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0xfe, 0xfe, 0x00, 0x00, 0x00, 0x00 }, 8);
	tmp = fahrenheit(calc_value(answer, 4));
	debug_print("temp: %.4f / expected: 32.0000\n", tmp);

	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0x23, 0x23, 0x23, 0x90, 0x00, 0x00 }, 8);
	tmp = fahrenheit(calc_value(answer, 4));
	debug_print("temp: %.4f / expected: 96.0125\n", tmp);

	device.conversion_method = 2;

	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0x09, 0x66, 0x00, 0x00, 0x00, 0x00 }, 8);
	tmp = calc_value(answer, 2);
	debug_print("temp: %.4f / expected: 24.0600\n", tmp);

	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0xf6, 0x9a, 0x00, 0x00, 0x00, 0x00 }, 8);
	tmp = calc_value(answer, 2);
	debug_print("temp: %.4f / expected: -24.0600\n", tmp);

	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0x05, 0x90, 0x00, 0x00, 0x00, 0x00 }, 8);
	tmp = calc_value(answer, 2);
	debug_print("temp: %.4f / expected: 14.2400\n", tmp);

	memmove(answer, (unsigned char[8]){ 0x80, 0x04, 0xfa, 0x70, 0x00, 0x00, 0x00, 0x00 }, 8);
	tmp = calc_value(answer, 2);
	debug_print("temp: %.4f / expected: -14.2400\n", tmp);

	/* some values provided by Samuel Progin from TEMPer V1.4:
	 * 80 02 19 30 65 72 46 31
	 * 80 02 19 40 65 72 46 31
	 * 80 02 19 50 65 72 46 31
	 * 80 02 19 60 65 72 46 31
	 * 80 02 1a 90 65 72 46 31
	 * Temperature sensor seems to be starting at offset 2
	 * probably the byte at offset 1 defines where the sensor values can be read?
	 * probably some other bytes define what/how many sensors are present?
	 * Compare to TEMPer V1.3:
	 * 80 04 06 06 06 f0 00 00
	 * Compare to TEMPerHUM (TEMPerX_V3.3)
	 * 80 40 0a f9 14 63 00 00
	 * 80 40 0b 5c 14 94 00 00
	 * Offset 0: constant 80 - could mean one temperature sensor?
	 * Offset 1: 04 at V1.3 - this is the offset where temperature sensor has it's values
	 *           02 at V1.4 - this is the offset where temperature sensor has it's values
	 *           40 at TEMPerHUM (TEMPerX_V3.3) - no idea what this could mean...
	 * Offset 2: V1.3: has always the same value as offset 3 and 4
	 *           V1.4: first byte of value
	 * Offset 3: V1.3: has always the same value as offset 2 and 4
	 *           V1.4: second byte of value
	 * Offset 4: V1.3: first byte of value
	 *           V1.4: constant 65
	 * Offset 5: V1.3: second byte of value
	 *           V1.4: constant 72
	 * Offset 6: V1.3: constant 00
	 *           V1.4: constant 46
	 * Offset 7: V1.3: constant 00
	 *           V1.4: constant 31
	 */
	device.conversion_method = 1;

	memmove(answer, (unsigned char[8]){ 0x80, 0x02, 0x1a, 0x90, 0x65, 0x72, 0x46, 0x31 }, 8);
	tmp = calc_value(answer, 2);
	debug_print("temp: %.4f / expected: 26.5625\n", tmp);

	exit(EXIT_SUCCESS);
}

/*
 * main
 *
 */

int main(int argc, char **argv)
{
	parse_parameters(argc, argv);

	if (!get_devnode())
	{
		cleanup();
		exit(EXIT_FAILURE);
	}
	if (!init_device())
	{
		cleanup();
		exit(EXIT_FAILURE);
	}
	if (!evaluate_device_details())
	{
		cleanup();
		exit(EXIT_FAILURE);
	}
	if (!query_values())
	{
		cleanup();
		exit(EXIT_FAILURE);
	}
	
	debug_print("Found firmware: '%s'\n", device.firmware);
	print_values(device.values[config.in_sensor], device.values[config.out_sensor], config.precision); 
	cleanup();
	exit(EXIT_SUCCESS);
}

/*
 * tempersensor Copyright (C) 2020-2021 Armin Fuerst (armin@fuerst.priv.at)
 * tempersensor is a complete rewrite based on the ideas from
 * pcsensor.c which was written by Juan Carlos Perez (cray@isp-sl.com)
 * based on Temper.c by Robert Kavaler (kavaler@diva.com)
 *
 * This file is part of tempersensor.
 *
 * Tempersensor is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Tempersensor is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with tempersensor.  If not, see <https://www.gnu.org/licenses/>.
 *
 *
 * Why did I put the license notice at the bottom of the file?
 * Because most of the time, when you open the file, the probability
 * you want to know about the license is very little. If you open the
 * file to learn about the license, it is still easy to find.
 *
 * Perhaps you want to have a look at the talk "Clean Coders Hate What 
 * Happens to Your Code When You Use These Enterprise Programming Tricks"
 * from Kevlin Henney at the NDC Conferences in London, January 16th-20th 
 * 2017. While watching the whole talk is a good idea, starting at around 
 * 27:50 provides some input what to put on the top of source code files.
 */ 
