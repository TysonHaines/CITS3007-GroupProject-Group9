#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "asset_validators.h"

/**
 * Validates that the name length is non-zero
 * Returns BUN_MALFORMED if the name length is zero, otherwise returns BUN_OK
*/
bun_result_t validate_name_length(u32 name_length) {
  if (name_length == 0) {
    fprintf(stderr, "\nname length must be non-zero\n");
    return BUN_MALFORMED;
  }
  return BUN_OK;
}

/**
 * Validates asset name fits in string table
 * Returns BUN_MALFORMED if name offset and length indicate that name does not fit in string table
 * otherwise returns BUN_OK
 */
bun_result_t name_fits_string_table(u32 name_offset, u64 string_table_size, u32 name_length) {
  if (name_offset > string_table_size || (u64)name_offset + (u64)name_length > string_table_size) {
    fprintf(stderr, "\nname is too large for string table\n");
    return BUN_MALFORMED;
  }
  return BUN_OK;
}

/**
 * Validates data fits in data section
 * Returns BUN_MALFORMED if data offset and size indicate that data does not fit
 * otherwise returns BUN_OK
 */
bun_result_t data_fits_data_section(u64 data_offset, u64 data_size, const BunHeader *header) {
  if (data_offset + data_size > header->data_section_size) {
    fprintf(stderr, "\ndata is too large for data section\n");  
    return BUN_MALFORMED;
  }
  return BUN_OK;
}

/**
 * Validates compression parameters
 * Returns BUN_MALFORMED if:
 * compression type is none, RLE, or zlib but uncompressed size is zero, or
 * compression type is RLE but data size is not even, or
 * compression type is RLE but any RLE count is zero, or
 * compression type is unknown
 * otherwise returns BUN_OK
 */
bun_result_t validate_compression(u32 compression, u64 uncompressed_size, u64 data_size, u64 data_offset, BunParseContext *ctx, const BunHeader *header) {
  if (compression == BUN_COMPRESS_NONE) {
      if (uncompressed_size != 0) {
        fprintf(stderr, "\nuncompressed size must be 0 for no compression\n");
        return BUN_MALFORMED;
      }
    }

    //if compression is RLE, uncompressed size must not be 0
    else if (compression == BUN_COMPRESS_RLE) {
      if (uncompressed_size == 0) {
        fprintf(stderr, "\nuncompressed size must be non-zero for RLE compression\n");
        return BUN_MALFORMED;
      }
      //check RLE data has even no. of bytes
      if (data_size % 2 != 0) {
        fprintf(stderr, "\nRLE data must have even number of bytes\n");
        return BUN_MALFORMED;
      }
      // guard against overflow when computing actual offset
      u64 actual_offset = header->data_section_offset + data_offset;
      if (actual_offset > (u64)ctx->file_size || data_size > (u64)ctx->file_size - actual_offset) {
        fprintf(stderr, "\ndata offset and size too large\n");
        return BUN_MALFORMED;
      }
      long current_pos = ftell(ctx->file);
        if (fseek(ctx->file, (long)actual_offset, SEEK_SET) != 0) {
          fprintf(stderr, "\nfailed to jump to data offset\n");
          return BUN_ERR_IO;
      }
      //  check rle count is non-zero
      for(u64 j = 0; j < data_size; j += 2) {
        u8 count_buf[2];
        if(fread(count_buf,1,2,ctx->file) != 2) {
          fprintf(stderr, "\nfailed to read RLE count\n");
          return BUN_ERR_IO;
        }
        if (count_buf[0] == 0) {
          fprintf(stderr, "\nRLE count must be non-zero\n");
          return BUN_MALFORMED;
        }
      }
      //restore file position
      if (fseek(ctx->file, current_pos, SEEK_SET) != 0) {
        fprintf(stderr, "\nfailed to restore file position\n");
        return BUN_ERR_IO;
      }
    }
    
    //if compression is zlib
    else if (compression == BUN_COMPRESS_ZLIB) {
      if (uncompressed_size == 0) {
        fprintf(stderr, "\nuncompressed size must be non-zero for zlib compression\n");
        return BUN_MALFORMED;
      }
      return BUN_UNSUPPORTED;
    }
    //if compression is unknown 
    else {
      fprintf(stderr, "\ncompression type unknown\n");
      return BUN_MALFORMED;
    }
    return BUN_OK;
}

/**
 * Validates checksum is non-zero
 * Returns BUN_UNSUPPORTED if checksum is zero
 * otherwise returns BUN_OK
 */
bun_result_t validate_non_zero_checksum(u32 checksum) {
  if (checksum != 0) {
    fprintf(stderr, "\nchecksum must be 0\n");
    return BUN_UNSUPPORTED;
  }
  return BUN_OK;
}

/**
 * Validates flags are either encrypted or executable
 * Returns BUN_UNSUPPORTED if flag is neither
 * otherwise returns BUN_OK
 */
bun_result_t validate_flags(u32 flags) {
  u32 known_flags = BUN_FLAG_ENCRYPTED | BUN_FLAG_EXECUTABLE;
  if (flags & ~known_flags) {
    fprintf(stderr, "\nflags contain unknown bits\n");
    return BUN_UNSUPPORTED;
  }
  return BUN_OK;
}

/**
 * validate asset names consist of only printable ASCII characters (0x20-0x7E)
 * Returns BUN_MALFORMED if asset name contains non-printable ASCII characters
 * Returns BUN_ERR_IO if name cannot be read
 * otherwise returns BUN_OK
 */
bun_result_t validate_asset_name(const BunHeader *header, BunParseContext *ctx, u32 name_offset, u32 name_length, char *asset_name) {
  // find address of asset name in string table
    u64 pos = (u64)header->string_table_offset + (u64)name_offset;

    // allocate memory for name buffer
    u8 *name_buf = malloc(name_length);

    // if memory allocation fails, return error
    if (!name_buf) { 
      fprintf(stderr, "\nfailed to allocate memory for 'name_buf'\n");
      return BUN_ERR_NOMEM;
    }

    // seek to position of name in string table and read name into buffer
    fseek(ctx->file, (long)pos, SEEK_SET);

    // failed to read name bytes
    //_____
    u32 read_len = name_length < 60 ? name_length : 60;
    //_____
    if (fread(name_buf, 1, read_len, ctx->file) != read_len) {
      free(name_buf);
      asset_name[read_len] = '\0';
      fprintf(stderr, "\nfailed to read name bytes\n");
      return BUN_ERR_IO; 
    }
    //_____
    for (size_t j = 0; j < name_length; j++) {
      if (name_buf[j] < 0x20 || name_buf[j] > 0x7E) {
        free(name_buf);
        fprintf(stderr, "\nname contains non-printable ASCII characters\n");
        return BUN_MALFORMED;
      }
    }
    free(name_buf);

    if (fseek(ctx->file, (long)pos, SEEK_SET) != 0) {
      fprintf(stderr, "\nfailed to find address of name in string table\n");
      return BUN_ERR_IO;
    }
    return BUN_OK;
}