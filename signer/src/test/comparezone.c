/*
 * Copyright (c) 2018 NLNet Labs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <ldns/ldns.h>

#include "comparezone.h"

static int
inputzone(const char* fname, ldns_zone** zoneptr, ldns_rr*** records, int* count)
{
    FILE* fp;
    ldns_rr* soa;
    ldns_rr_list* wsoa;
    ldns_rr_list* rrl1 = NULL;
    ldns_rr_list* rrl2 = NULL;
    ldns_zone* zone = NULL;
    ldns_status status;
    ldns_rr** rrs;
    ldns_rr* rr;
    int linenum;
    int failure = 0;
    size_t i;

    fp = fopen(fname, "r");
    if (fp) {
        status = ldns_zone_new_frm_fp_l(&zone, fp, NULL, 0, LDNS_RR_CLASS_IN, &linenum);
        if (status != LDNS_STATUS_OK) {
            failure = 1;
        }
        fclose(fp);
    } else {
        failure = 1;
    }
    for(i=0; i<ldns_rr_list_rr_count(ldns_zone_rrs(zone)); i++) {
        ldns_rr2canonical(ldns_rr_list_rr(ldns_zone_rrs(zone), i));
    }
    ldns_zone_sort(zone);
    rrl1 = ldns_zone_rrs(zone);

    rrl2 = ldns_rr_list_clone(rrl1);
    soa = ldns_zone_soa(zone);
    if(soa) {
        ldns_rr2canonical(soa);
        ldns_rr_list_push_rr(rrl2, ldns_rr_clone(ldns_zone_soa(zone)));
    }
    ldns_rr_list_sort(rrl2);
    *count = ldns_rr_list_rr_count(rrl2);

    rrs = malloc(sizeof(ldns_rr*) * ldns_rr_list_rr_count(rrl2));
    i = 0;
    do {
        rr = ldns_rr_list_pop_rr(rrl2);
        if(rr)
            rrs[i++] = rr;
    } while(rr);
    ldns_rr_list_free(rrl2);
    //ldns_rr_list_free(rrl1);

    *records = rrs;
    *zoneptr = zone;
    return failure;
}

static int
skiprr(ldns_rr* rr, int flags)
{
    if(rr == 0)
            return 0;
    switch(ldns_rr_get_type(rr)) {
        case LDNS_RR_TYPE_RRSIG:
        case LDNS_RR_TYPE_NSEC:
        case LDNS_RR_TYPE_NSEC3:
        case LDNS_RR_TYPE_NSEC3PARAM:
        case LDNS_RR_TYPE_DNSKEY:
            return 1;
        case LDNS_RR_TYPE_SOA:
            if(flags & comparezone_INCL_SOA)
                return 0;
            else
                return 1;
        default:
            return 0;
    }
}

int
comparerr(ldns_rr* rr1, ldns_rr* rr2)
{
    int cmp = 0;
    if (rr1 == NULL) {
        cmp = 1;
    } else if(rr2 == NULL) {
        cmp = -1;
    }
    if (cmp == 0) {
        cmp = ldns_dname_compare(ldns_rr_owner(rr1), ldns_rr_owner(rr2));
    }
    if (cmp == 0) {
        cmp = ldns_rr_compare(rr1, rr2);
    }
    if (cmp == 0) {
        cmp = ldns_rr_ttl(rr1) - ldns_rr_ttl(rr2);
    }
    return cmp;
}

int
comparezone(const char* fname1, const char* fname2, int flags)
{
    FILE* fp;
    char* s;
    int i, j;
    int count1, count2;
    ldns_rr** array1;
    ldns_rr** array2;
    ldns_rr* rr1 = NULL;
    ldns_rr* rr2 = NULL;
    ldns_zone *z1 = NULL;
    ldns_zone *z2 = NULL;
    int failure = 0;
    int cmp;
    int differences = 0;
    
    failure |= inputzone(fname1, &z1, &array1, &count1);
    failure |= inputzone(fname2, &z2, &array2, &count2);

    for (i=0, j=0; i<count1 || j<count2; ) {
        cmp = 0;
        if (i<count1 && j<count2) {
            rr1 = array1[i];
            rr2 = array2[j];
            cmp = comparerr(rr1, rr2);
        } else if (i >= count1) {
            rr1 = NULL;
            rr2 = array2[j];
            cmp = 1;
        } else if (j >= count2) {
            rr1 = array1[i];
            rr2 = NULL;
            cmp = -1;
        }
        if (cmp == 0) {
            i += 1;
            j += 1;
        } else {
            if (i>0 && !comparerr(rr1,array1[i-1])) {
                i += 1;
            } else if (j>0 && !comparerr(rr2,array2[j-1])) {
                j += 1;
            } else if(skiprr(rr1,flags)) {
                i += 1;
            } else if(skiprr(rr2,flags)) {
                j += 1;
            } else if (cmp < 0) {
                s = ldns_rr2str(rr1);
                fprintf(stderr,"- %s",s);
                free(s);
                i += 1;
                differences += 1;
            } else if (cmp > 0) {
                s = ldns_rr2str(rr2);
                fprintf(stderr,"+ %s",s);
                free(s);
                j += 1;
                differences += 1;
            }
        }
    }

    if(differences != 0) {
        failure |= 1;
    }
    if(array1) {
        for(i=0; i<count1; i++)
            ldns_rr_free(array1[i]);
        free(array1);
    }
    if(array2) {
        for(i=0; i<count2; i++)
            ldns_rr_free(array2[i]);
        free(array2);
    }
    if(z1)
        ldns_zone_deep_free(z1);
    if(z2)
        ldns_zone_deep_free(z2);

    return failure;
}
