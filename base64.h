// vim: sw=2 ts=2 expandtab smartindent

#ifndef base64_IMPLEMENTATION
static void fbase64(
  FILE *fp,
  unsigned char *data,
  size_t data_len
);
#endif

#ifdef base64_IMPLEMENTATION

static char char_base64(unsigned char in) {
  switch (in) {
    case  0: return 'A';
    case  1: return 'B';
    case  2: return 'C';
    case  3: return 'D';
    case  4: return 'E';
    case  5: return 'F';
    case  6: return 'G';
    case  7: return 'H';
    case  8: return 'I';
    case  9: return 'J';
    case 10: return 'K';
    case 11: return 'L';
    case 12: return 'M';
    case 13: return 'N';
    case 14: return 'O';
    case 15: return 'P';
    case 16: return 'Q';
    case 17: return 'R';
    case 18: return 'S';
    case 19: return 'T';
    case 20: return 'U';
    case 21: return 'V';
    case 22: return 'W';
    case 23: return 'X';
    case 24: return 'Y';
    case 25: return 'Z';
    case 26: return 'a';
    case 27: return 'b';
    case 28: return 'c';
    case 29: return 'd';
    case 30: return 'e';
    case 31: return 'f';
    case 32: return 'g';
    case 33: return 'h';
    case 34: return 'i';
    case 35: return 'j';
    case 36: return 'k';
    case 37: return 'l';
    case 38: return 'm';
    case 39: return 'n';
    case 40: return 'o';
    case 41: return 'p';
    case 42: return 'q';
    case 43: return 'r';
    case 44: return 's';
    case 45: return 't';
    case 46: return 'u';
    case 47: return 'v';
    case 48: return 'w';
    case 49: return 'x';
    case 50: return 'y';
    case 51: return 'z';
    case 52: return '0';
    case 53: return '1';
    case 54: return '2';
    case 55: return '3';
    case 56: return '4';
    case 57: return '5';
    case 58: return '6';
    case 59: return '7';
    case 60: return '8';
    case 61: return '9';
    case 62: return '+';
    case 63: return '/';
    default: return '?';
  }
}

static void fbase64(
  FILE *fp,
  unsigned char *data,
  size_t data_len
) {
  for (int i = 0; i < data_len; i += 3) {
    uint32_t input = 0;
    if ((i + 0) < data_len) input |= (data[i + 0] << 16);
    if ((i + 1) < data_len) input |= (data[i + 1] <<  8);
    if ((i + 2) < data_len) input |= (data[i + 2] <<  0);
    size_t bytes_available = ((i + 0) < data_len) +
                             ((i + 1) < data_len) +
                             ((i + 2) < data_len);

    char x1, x2, x3, x4;
    x1 = char_base64((input >> 18) & 63);
    x2 = char_base64((input >> 12) & 63);
    x3 = char_base64((input >>  6) & 63);
    x4 = char_base64((input >>  0) & 63);

    switch (bytes_available) {
      case 3: fprintf(fp, "%c%c%c%c", x1, x2, x3, x4); break;
      case 2: fprintf(fp, "%c%c%c=" , x1, x2, x3    ); break;
      case 1: fprintf(fp, "%c%c=="  , x1, x2        ); break;
    }
  }
}

#endif
