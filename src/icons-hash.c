/*
 * simple program which outputs a hash-table of `icons_ext` with low collusion.
 * the hash function is case-insensitive, it also doesn't hash beyond the
 * length of the longest extension.
 */

#include <stddef.h>
#include <stdint.h>

#define GOLDEN_RATIO_16 40503u /* golden ratio for 16bits: (2^16) / 1.61803 */
#define ICONS_TABLE_SIZE 8 /* size in bits. 8 = 256 */

#ifdef ICONS_GENERATE

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "icons.h"

#ifdef NDEBUG
	#error "NDEBUG"
#endif

#define ASSERT(X) assert(X)
#define ARRLEN(X) (sizeof(X) / sizeof((X)[0]))
#define MAX(A, B) ((A) > (B) ? (A) : (B))
#define HGEN_ITERARATION (1ul << 14)
#define ICONS_PROBE_MAX_ALLOWED 6
#define ICONS_MATCH_MAX ((size_t)-1)

#if 0 /* enable for debugging */
	#define log(...)  fprintf(stderr, "[INFO]: " __VA_ARGS__)
#else
	#define log(...) ((void)0)
#endif

static uint16_t icon_ext_hash(const char *s);

/* change ICONS_TABLE_SIZE to increase the size of the table */
static struct icon_pair table[1u << ICONS_TABLE_SIZE];
static uint16_t seen[ARRLEN(table)];
/* arbitrarily picked starting position. change if needed.
 * but ensure they're above 1 and prefer prime numbers.
 */
static uint16_t hash_start = 7;
static uint16_t hash_mul = 251;

/*
 * use robin-hood insertion to reduce the max probe length
 */
static void
rh_insert(const struct icon_pair item, uint32_t idx, uint32_t n)
{
	assert(n != 0);
	for (uint32_t tries = 0; tries < ARRLEN(table); ++tries, ++n) {
		if (seen[idx] < n) {
			struct icon_pair tmp_item = table[idx];
			uint32_t tmp_n = seen[idx];

			table[idx] = item;
			seen[idx] = n;

			if (tmp_n != 0) /* the slot we inserted to wasn't empty */
				rh_insert(tmp_item, idx, tmp_n);
			return;
		}
		idx = (idx + 1) % ARRLEN(table);
	}
	assert(0); /* unreachable */
}

static unsigned int
table_populate(void)
{
	memset(seen, 0x0, sizeof seen);
	memset(table, 0x0, sizeof table);
	for (size_t i = 0; i < ARRLEN(icons_ext); ++i) {
		if (icons_ext[i].icon[0] == '\0') /* skip empty entries */
			continue;
		uint32_t h = icon_ext_hash(icons_ext[i].match);
		rh_insert(icons_ext[i], h, 1);
	}

	unsigned int max_probe = 0;
	for (size_t i = 0; i < ARRLEN(seen); ++i)
		max_probe = MAX(max_probe, seen[i]);
	return max_probe;
}

int
main(void)
{
	assert(ARRLEN(icons_ext) <= ARRLEN(table));
	assert(ICONS_TABLE_SIZE < 16); /* the hash function only supports upto 16 bits */
	assert(1u << ICONS_TABLE_SIZE == ARRLEN(table));
	assert((GOLDEN_RATIO_16 & 1) == 1); /* must be odd */
	assert(hash_start > 1); assert(hash_mul > 1);
	/* ensure power of 2 hashtable size which allows compiler to optimize
	 * away mod (`%`) operations
	 */
	assert((ARRLEN(table) & (ARRLEN(table) - 1)) == 0);

	unsigned int max_probe = (unsigned)-1;
	uint16_t best_hash_start, best_hash_mul;

	for (size_t i = 0; i < HGEN_ITERARATION; ++i) {
		unsigned z = table_populate();
		if (z < max_probe) {
			max_probe = z;
			best_hash_start = hash_start;
			best_hash_mul = hash_mul;
		}
		hash_start *= GOLDEN_RATIO_16;
		hash_mul *= GOLDEN_RATIO_16;
	}
	assert(max_probe < ICONS_PROBE_MAX_ALLOWED);
	hash_start = best_hash_start;
	hash_mul = best_hash_mul;
	{
		unsigned tmp = table_populate();
		assert(tmp == max_probe);
	}

	/* sanity check */
	for (size_t i = 0; i < ARRLEN(icons_ext); ++i) {
		if (icons_ext[i].icon[0] == 0) continue;
		uint16_t found = 0, h = icon_ext_hash(icons_ext[i].match);
		for (uint16_t k = 0; k < max_probe; ++k) {
			uint16_t z = (h + k) % ARRLEN(table);
			if (table[z].match && strcmp(icons_ext[i].match, table[z].match) == 0) {
				found = 1;
			}
		}
		assert(found);
	}

	log("hash_start: %6u\n", (unsigned)hash_start);
	log("hash_mul  : %6u\n", (unsigned)hash_mul);
	log("max_probe : %6u\n", max_probe);

	size_t match_max = 0, icon_max = 0;
	for (size_t i = 0; i < ARRLEN(icons_name); ++i) {
		match_max = MAX(match_max, strlen(icons_name[i].match) + 1);
		icon_max = MAX(icon_max, strlen(icons_name[i].icon) + 1);
	}
	for (size_t i = 0; i < ARRLEN(icons_ext); ++i) {
		match_max = MAX(match_max, strlen(icons_ext[i].match) + 1);
		icon_max = MAX(icon_max, strlen(icons_ext[i].icon) + 1);
	}
	icon_max = MAX(icon_max, strlen(dir_icon.icon) + 1);
	icon_max = MAX(icon_max, strlen(exec_icon.icon) + 1);
	icon_max = MAX(icon_max, strlen(file_icon.icon) + 1);

	const char *uniq[ARRLEN(icons_ext)] = {0};
	size_t uniq_head = 0;
	for (size_t i = 0; i < ARRLEN(icons_ext); ++i) {
		if (icons_ext[i].icon[0] == 0) continue;
		int isuniq = 1;
		for (size_t k = 0; k < uniq_head; ++k) {
			if (strcmp(uniq[k], icons_ext[i].icon) == 0) {
				isuniq = 0;
				break;
			}
		}
		if (isuniq) {
			assert(uniq_head < ARRLEN(uniq));
			uniq[uniq_head++] = icons_ext[i].icon;
		}
	}
	assert(uniq_head < (unsigned char)-1);

	log("uniq icons: %6zu\n", uniq_head);
	log("no-compact: %6zu bytes\n", ARRLEN(table) * icon_max);
	log("compaction: %6zu bytes\n", uniq_head * icon_max + ARRLEN(table));

	printf("#ifndef INCLUDE_ICONS_GENERATED\n");
	printf("#define INCLUDE_ICONS_GENERATED\n\n");

	printf("/*\n * NOTE: This file is automatically generated.\n");
	printf(" * DO NOT EDIT THIS FILE DIRECTLY.\n");
	printf(" * Use `icons.h` to customize icons\n */\n\n");

	printf("#define hash_start %uu\n", hash_start);
	printf("#define hash_mul   %uu\n\n", hash_mul);
	printf("#define ICONS_PROBE_MAX %u\n", max_probe);
	printf("#define ICONS_MATCH_MAX %zuu\n\n", match_max);

	printf("struct icon_pair { const char match[%zu]; const char icon[%zu]; unsigned char color; };\n\n",
	       match_max, icon_max);

	printf("static const char icons_ext_uniq[%zu][%zu] = {\n", uniq_head, icon_max);
	for (size_t i = 0; i < uniq_head; ++i)
		printf("\t\"%s\",\n", uniq[i]);
	printf("};\n\n");

	printf("static const struct {\n\tconst char match[%zu];"
	       "\n\tunsigned char idx;\n\tunsigned char color;\n} icons_ext[%zu] = {\n",
	        match_max, ARRLEN(table));
	for (size_t i = 0; i < ARRLEN(table); ++i) {
		if (table[i].icon == NULL || table[i].icon[0] == '\0') /* skip empty entries */
			continue;
		int k;
		for (k = 0; k < uniq_head; ++k) if (strcmp(table[i].icon, uniq[k]) == 0) break;
		assert(k < uniq_head);
		printf("\t[%3zu] = {\"%s\", %d, %hhu },\n",
		       i, table[i].match, k, table[i].color);
	}
	printf("};\n\n");

	printf("#endif /* INCLUDE_ICONS_GENERATED */\n");
}

#else
	#define ASSERT(X) ((void)0)
#endif /* ICONS_GENERATE */

#ifndef TOUPPER
	#define TOUPPER(ch)     (((ch) >= 'a' && (ch) <= 'z') ? ((ch) - 'a' + 'A') : (ch))
#endif

#if defined(ICONS_GENERATE) || defined(ICONS_ENABLED)
static uint16_t
icon_ext_hash(const char *str)
{
	uint32_t i, hash = hash_start;
	const unsigned int z = (sizeof hash * CHAR_BIT) - ICONS_TABLE_SIZE;
	for (i = 0; i < ICONS_MATCH_MAX && str[i] != '\0'; ++i) {
		hash ^= TOUPPER((unsigned char)str[i]);
		hash *= hash_mul;
	}
	hash ^= (hash >> z);
	hash *= GOLDEN_RATIO_16;
	hash >>= z;
	ASSERT(hash < ARRLEN(table));
	return hash;
}
#endif
