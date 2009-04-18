#include "fileexists.h"

#include <stdio.h>

bool fileExists(const std::string & n)
{
  FILE *f = fopen(n.c_str(), "r");

  if (f)
  {
    fclose(f);
    return true;
  }
  else
  {
    return false;
  }
}

