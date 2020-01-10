static inline void
_push16(unsigned char *out, size_t *i_p, uint16_t v)
{
    out[0 + *i_p] = (unsigned char) (v      );
    out[1 + *i_p] = (unsigned char) (v >>  8);
    (*i_p) += 2;
}

static inline void
_push64(unsigned char *out, size_t *i_p, uint64_t v)
{
    out[0 + *i_p] = (unsigned char) (v      );
    out[1 + *i_p] = (unsigned char) (v >>  8);
    out[2 + *i_p] = (unsigned char) (v >> 16);
    out[3 + *i_p] = (unsigned char) (v >> 24);
    out[4 + *i_p] = (unsigned char) (v >> 32);
    out[5 + *i_p] = (unsigned char) (v >> 40);
    out[6 + *i_p] = (unsigned char) (v >> 48);
    out[7 + *i_p] = (unsigned char) (v >> 56);
    (*i_p) += 8;
}

static inline void
_push128(unsigned char *out, size_t *i_p, const unsigned char v[16])
{
    memcpy(&out[*i_p], v, 16);
    (*i_p) += 16;
}

static inline void
_push256(unsigned char *out, size_t *i_p, const unsigned char v[32])
{
    memcpy(&out[*i_p], v, 32);
    (*i_p) += 32;
}

static inline void
_pop16(uint16_t *v, const unsigned char *in, size_t *i_p)
{
    *v = (((uint16_t) in[0 + *i_p])      )
       | (((uint16_t) in[1 + *i_p]) <<  8);
    (*i_p) += 2;
}

static inline void
_pop64(uint64_t *v, const unsigned char *in, size_t *i_p)
{
    *v = (((uint64_t) in[0 + *i_p])      )
       | (((uint64_t) in[1 + *i_p]) <<  8)
       | (((uint64_t) in[2 + *i_p]) << 16)
       | (((uint64_t) in[3 + *i_p]) << 24)
       | (((uint64_t) in[4 + *i_p]) << 32)
       | (((uint64_t) in[5 + *i_p]) << 40)
       | (((uint64_t) in[6 + *i_p]) << 48)
       | (((uint64_t) in[7 + *i_p]) << 56);
    (*i_p) += 8;
}

static inline void
_pop128(unsigned char v[32], const unsigned char *in, size_t *i_p)
{
    memcpy(v, &in[*i_p], 16);
    (*i_p) += 16;
}

static inline void
_pop256(unsigned char v[32], const unsigned char *in, size_t *i_p)
{
    memcpy(v, &in[*i_p], 32);
    (*i_p) += 32;
}
