/* simple program to convert a datafile to C code */

#include <stdio.h>

#include <vector>

#include <FL/Fl_Shared_Image.H>

void printStringChar(FILE *out, unsigned char c) {
#if 0
  if (c >= ' ' && c <= '~' && c != '\\' && c != '\"')
    fprintf(out, "%c", c);
  else if (c == 0xA)
    fprintf(out, "\\n");
  else if (c == 0xD)
    fprintf(out, "\\r");
  else if (c == '\"')
    fprintf(out, "\\\"");
  else if (c == '\\')
    fprintf(out, "\\\\");
  else if (c == 0)
    fprintf(out, "\\0");
  else 
#endif
    fprintf(out, "\\x%02x", c);
}

#ifdef WIN32
const char * basename(const char * n) {

  size_t pos = strlen(n)-1;

  while (pos) {
    if (n[pos] == '/' || n[pos] == '\\')
      return n+pos;
    pos--;
  }

  return n;
}
#endif


/* first param: output file name
 * following param: input files
 * until param "-"
 * following param: images files
 */

typedef struct {

  int w, h;
  char * name;

} imageStruct;

int main(int argv, char * args[]) {

  FILE * out = fopen(args[1], "w");

  int argpos = 2;

  std::vector<char*> fnames;

  int fnum = 0;

  fprintf(out, "#include \"helpdata.h\"\n\n");

  while (argpos < argv) {

    if (args[argpos][0] == '-') {
      argpos++;
      break;
    }

    FILE * in = fopen(args[argpos], "r");

    if (in) {

      int col = 0;

      fnames.push_back(args[argpos]);

      fprintf(out, "const char file%02i[] =\n  \"", fnum++);

      while (!feof(in)) {

        int c = fgetc(in);

	if (col >= 70) {
	  fprintf(out, "\"\n  \"");
	  col = 0;
	}

	if (!feof(in))
	  printStringChar(out, c);

	col++;
      }

      fprintf(out, "\";\n\n");

      fclose(in);
    }

    argpos++;
  }

  fnum = 0;

  fl_register_images();

  std::vector<imageStruct> inames;

  while (argpos < argv) {

    if (args[argpos][0] == '-') {
      argpos++;
      break;
    }
    
    Fl_Shared_Image * in = Fl_Shared_Image::get(args[argpos]);

    if (in) {

      int col = 0;

      imageStruct is;
      is.name = args[argpos];
      is.w = in->w();
      is.h = in->h();

      inames.push_back(is);

      fprintf(out, "const unsigned char image%02i[] = \n  \"", fnum++);

      printf("%s:   w: %i h: %i count: %i, ld: %i, d: %i\n",args[argpos], in->w(), in->h(), in->count(), in->ld(), in->d());

      for (int i = 0; i < in->w()*in->h()*in->d(); i++) {

        int c = in->data()[0][i];

	if (col >= 30) {
	  fprintf(out, "\"\n  \"");
	  col = 0;
	}

        printStringChar(out, c);

	col++;
      }

      fprintf(out, "\";\n\n");
    }

    argpos++;
  }

  fprintf(out, "filestruct filelist[] = {\n");

  for (int i = 0; i < fnames.size(); i++) {
    fprintf(out, "  { \"%s\", file%02i, sizeof(file%02i) },\n", basename(fnames[i]), i, i);
  }

  fprintf(out, "  {0, 0, 0}\n};\n");

  fprintf(out, "imagestruct imagelist[] = {\n");

  for (int i = 0; i < inames.size(); i++) {
    fprintf(out, "  { \"%s\", image%02i, %i, %i},\n", basename(inames[i].name), i, inames[i].w, inames[i].h);
  }

  fprintf(out, "  {0, 0, 0, 0}\n};\n");

  fclose(out);
}
