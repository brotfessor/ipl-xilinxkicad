#include <sexp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <getopt.h>

//column numbers inside csv file
#define CSV_COL_PINNUM 1
#define CSV_COL_MINDELAY 4
#define CSV_COL_MAXDELAY 5

static int quiet = 0;

static int csv_getval(uint8_t **loc, uint8_t *buffer, int line, int col) {
  int location = 0;
  int linecounter = 0;
  int colcounter = 0;
  
  while(linecounter != line) {
    if(buffer[location] == 0)
      return -1;
    if(buffer[location] == '\n')
      linecounter++;
    location++;
  }

  while(colcounter != col) {
    if(buffer[location] == 0)
      return -2;
    if(buffer[location] == ',')
      colcounter++;
    location++;
  }

  *loc = &buffer[location];

  while(buffer[location] != ',') {
    if(buffer[location] == 0)
      return -3;
    location++;
  }

  return &buffer[location] - *loc;
}

static int csv_findline(uint8_t *buffer, int col, uint8_t *value) {
  int retval;
  uint8_t *pos;
  for(int line = 0;; line++) {
    retval = csv_getval(&pos, buffer, line, col);
    if(retval == -1)
      return -1;
    else if(retval == -2 || retval == -3)
      continue;
    else if(!strncmp(value, pos, strlen(value)))
      return line;
  }
}

static double atof_len(uint8_t *str, int length) {
  uint8_t buffer[100];
  memcpy(buffer, str, length);
  buffer[length] = 0;
  return atof(buffer);
}

static double calc_equiv_tracelength(double mindelay, double maxdelay, double epsilon_r) {
  double propagationdelay = sqrt(5.32 * epsilon_r + 7.504); // ps/mm
  return (mindelay + maxdelay) / 2 / propagationdelay; //mm
}

static double get_equiv_tracelength(uint8_t *buffer, uint8_t *pinname, double epsilon_r) {
  uint8_t *pos;
  int line = csv_findline(buffer, CSV_COL_PINNUM, pinname);
  if(line < 0) {
    fprintf(stderr, "WARN: could not find entry for pin %s\n", pinname);
    return 0.0;
  }
  double mindelay = atof_len(pos, csv_getval(&pos, buffer, line, CSV_COL_MINDELAY));
  double maxdelay = atof_len(pos, csv_getval(&pos, buffer, line, CSV_COL_MAXDELAY));
  if(mindelay == 0.0 || maxdelay == 0.0) {
    if(!quiet)
      fprintf(stderr, "WARN: could not get delay entry for pin %s\n", pinname);
    return 0.0;
  }
  return calc_equiv_tracelength(mindelay, maxdelay, epsilon_r);
}

static int readfile(uint8_t **dest, uint8_t *filename) {
  FILE *f;
  long size;
  
  f = fopen(filename, "r");
  if(!f) {
    fprintf(stderr, "Error opening the file\n");
    return -1;
  }
  fseek(f, 0, SEEK_END);
  size = ftell(f);
  rewind(f);
  *dest = malloc(size + 1);
  if(!(*dest)) {
    fprintf(stderr, "Error malloc()ing\n");
    return(-1);
  }
  fread(*dest, 1, size, f);
  (*dest)[size] = 0;
  fclose(f);
  return 0;
}

static int writefile(uint8_t *buffer, uint8_t *filename) {
  FILE *f;
  f = fopen(filename, "w");
  if(!f) {
    fprintf(stderr, "Error opening the output file for write\n");
    return -1;
  }
  fputs(buffer, f);
  fclose(f);
}

static sexp_t *sexp_get_last(sexp_t *first) {
  while(first->next)
    first = first->next;
  return first;
}

static int add_pkg_delays(sexp_t *sx, uint8_t *csv, double epsilon_r) {
  if(sx->ty != SEXP_LIST ||
     sx->list->ty != SEXP_VALUE ||
     strcmp(sx->list->val, "module")) {
    fprintf(stderr, "not a valid footprint file\n");
    return -1;
  }

  sexp_t *entry;
  double length;
  sexp_t *propertyname;
  sexp_t *propertyval;
  sexp_t *property;
  
  for(entry = sx->list; entry; entry = entry->next) {
    if(entry->ty != SEXP_LIST ||
       entry->list->ty != SEXP_VALUE ||
       strcmp(entry->list->val, "pad"))
      continue;

    length = get_equiv_tracelength(csv, entry->list->next->val, epsilon_r);
    if(length == 0.0)
      continue;

    if(!quiet)
      fprintf(stderr, "Pin %s has an internal effective length of %f mm\n",
	      entry->list->next->val, length);

    propertyname = new_sexp_atom("die_length", strlen("die_length"), SEXP_BASIC);
    propertyval = new_sexp_atom("", 20, SEXP_BASIC);
    snprintf(propertyval->val, 20, "%f", length);
    propertyval->next = 0;
    propertyname->next = propertyval;
    property = new_sexp_list(propertyname);
    sexp_get_last(entry->list)->next = property;
  }
}

static const struct option long_options[] =
  {
   { "help", no_argument, 0, 'h' },
   { "input", required_argument, 0, 'i' },
   { "output", required_argument, 0, 'o' },
   { "csv", required_argument, 0, 'c' },
   { "epsilonr", required_argument, 0, 'e' },
   { "quiet", no_argument, 0, 'q' },
   {}
  };

void print_help(void) {
  fprintf(stderr, "ipl-xilinxkicad v1\n\n");
  fprintf(stderr, "Adds the internal package delays from a xilinx csv file to a kicad footprint\n");
  fprintf(stderr, "for proper trace length matching\n\n");
  fprintf(stderr, "--help     -h          This help\n");
  fprintf(stderr, "--input    -i  <file>  Input .kicad_mod file (required)\n");
  fprintf(stderr, "--csv      -c  <file>  Vivado-generated csv (required)\n");
  fprintf(stderr, "--output   -o  <file>  Output .kicad_mod file\n");
  fprintf(stderr, "--epsilonr -e  <value> Epsilon_r of your PCB material (default 4.05)\n");
  fprintf(stderr, "--quiet    -q          Be quiet\n");
}

int main(int argc, char **argv) {
  uint8_t *inputfilename = 0;
  uint8_t *outputfilename = 0;
  uint8_t *csvfilename = 0;
  double epsilonr = 4.05;
  
  uint8_t *pretty;
  uint8_t *prettyoutput;
  uint8_t *csv;
  sexp_t *sx;

  int opt;
  int index;
  while((opt = getopt_long(argc, argv, "hi:o:c:e:q", long_options, &index)) > 0) {
    switch(opt) {
    case 'h':
      print_help();
      return 0;
    case 'i':
      inputfilename = optarg;
      break;
    case 'o':
      outputfilename = optarg;
      break;
    case 'c':
      csvfilename = optarg;
      break;
    case 'e':
      epsilonr = atof(optarg);
      if(epsilonr == 0.0) {
	fprintf(stderr, "Epsilon_r is not a valid number\n");
	return -1;
      }
      break;
    case 'q':
      quiet = 1;
    default:
      break;
    }
  }

  if(!inputfilename) {
    fprintf(stderr, "You have to specify an input file\n");
    return -1;
  }
  if(!csvfilename) {
    fprintf(stderr, "You have to specify a csv file\n");
    return -1;
  }

  if(readfile(&pretty, inputfilename) ||
     readfile(&csv, csvfilename))
    exit(-1);

  sx = parse_sexp(pretty, strlen(pretty));
  if(!sx) {
    fprintf(stderr, "s-exp parsing failed\n");
    return -1;
  }

  prettyoutput = malloc(strlen(pretty)*2);
  if(!prettyoutput) {
    fprintf(stderr, "Error malloc()ing\n");
    return -1;
  }

  add_pkg_delays(sx, csv, epsilonr);

  print_sexp(prettyoutput, strlen(pretty)*2, sx);
  sexp_cleanup();
  
  if(!outputfilename)
    printf(prettyoutput);
  else
    writefile(prettyoutput, outputfilename);

  return 0;
}
  
