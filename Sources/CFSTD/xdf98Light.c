//
//  xdf98Light.c
//  
//
//  Created by Patrick Zippenfenig on 18.04.2024.
//

#include "xdf98Light.h"
#include "App.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


//! Calculates an address from an handle for a sequential file
//! \return Corresponding address
static int32_t address_from_handle(
    //! [in] (cluster:2, address:22, file_index:8)
    int handle,
    //! [in] Pointer to xdf file information structure
    file_table_entry *fte
) {
    int addr = (ADDRESS_FROM_HNDL(handle) << (2 * CLUSTER_FROM_HANDLE(handle)));
    if (fte->fstd_vintage_89) {
        addr = (addr * 15);
    }
    addr = W64TOwd(addr) + 1;
    return addr;
}



//! Find the position of the record as described by the given primary keys.
//
//! The search begins from the record pointed by handle.  If handle is 0,
//! the search is from beginning of file. If handle is -1, the search
//! begins from the current position. If a specific key has the value of
//! -1, this key will not used as a selection criteria.
//! If nprim is -1, the last selection criterias will be used for the
//! search. Upon completion a "pointer" to the record (handle) is returned.
int c_xdfloc(
    //! [in] Unit number associated to the file
    int iun,
    //! [in] Handle to the starting search position
    int handle,
    //! [in] Primary search keys
    uint32_t *primk,
    //! [in] Number search primary search keys
    int nprim
) {
  uint32_t *mskkeys = NULL;

    return c_xdfloc2(iun, handle, primk, nprim, mskkeys);
}




//! Calculates an handle for a sequential file from address and index
//! \return Sequential file handle
static int32_t make_seq_handle(
    //! [in] Address (in units of 32bit)
    int address,
    //! [in] File index in table
    int file_index,
    //! [in] Pointer to xdf file information structure
    file_table_entry *fte)
{
    int cluster, addr;
    static int MB512 = 0x4000000, MB128 = 0x1000000, MB32 = 0x400000;
    // MB512 , MB128 and MB32 are represented in 64 bit unit

    // 32 bit word to 64 bit unit address
    address = (address - 1) >> 1;
    if (fte->fstd_vintage_89) {
        cluster = 0;
        address = address / 15;
        addr = address;
    } else {
        if (address >= MB512) {
            cluster = 3;
            addr = address >> 6;
        } else {
            if (address >= MB128) {
                cluster = 2;
                addr = address >> 4;
            } else {
                if (address >= MB32) {
                    cluster = 1;
                    addr = address >> 2;
                } else {
                    cluster = 0;
                    addr = address;
                }
            }
        }
    }

    return MAKE_SEQ_HANDLE(cluster, addr, file_index);
}




//! Find the next record that matches the current search criterias.
//! \return Handle of the next matching record or error code
static uint32_t next_match(
    //! [in] index of file in the file table
    int file_index
) {
    register file_table_entry *f;
    int found, i, record_in_page, page_no, j, width, match;
    int end_of_file, nw, iun, addr_match;
    /*   xdf_record_header *rec; */
    uint32_t *entry, *search, *mask, handle;
    stdf_dir_keys *stds, *stdm, *stde;
    seq_dir_keys *seq_entry;
    xdf_record_header *header;
    int32_t f_datev;
    double nhours;
    int deet, npas, i_nhours, run, datexx;

    // check if file exists
    if ( (f = file_table[file_index]) == NULL) return (ERR_NO_FILE);

    iun = file_table[file_index]->iun;
    width = W64TOWD(f->primary_len);
    found = 0;
    f->valid_pos = 0;

    if (! f->xdf_seq) {
        // Check if there is a valid current page
        if (f->cur_dir_page == NULL) return (ERR_NO_POS);
        if (f->cur_entry == NULL) return (ERR_NO_POS);
        if (f->cur_entry - (f->cur_dir_page)->dir.entry != f->page_record *W64TOWD(f->primary_len)) {
            return (ERR_NO_POS);
        }

        f->page_nrecords = (f->cur_dir_page)->dir.nent;
        while (! found) {
            if (f->page_record >= f->page_nrecords ) {
                // No more records in page
                f->page_record = 0;
                f->page_nrecords = 0;
                // Position to next dir page
                f->cur_dir_page = (f->cur_dir_page)->next_page;
                if (f->cur_dir_page == NULL) {
                    // No more pages, end of file
                    break;
                }
                f->cur_pageno++;
                f->page_nrecords = (f->cur_dir_page)->dir.nent;
                f->cur_entry = (f->cur_dir_page)->dir.entry;
            } else {
                for (j = f->page_record; (j < f->page_nrecords) && !found; j++) {
                    entry = f->cur_entry;
                    header = (xdf_record_header *) entry;
                    if (header->idtyp < 127) {
                        search = (uint32_t *) f->target;
                        mask = (uint32_t *) f->cur_mask;
                        stde = (stdf_dir_keys *) entry;
                        stdm = (stdf_dir_keys *) mask;
                        stds = (stdf_dir_keys *) search;
                        match = 0;
                        for (i = 0; i < width; i++, mask++, search++, entry++) {
                            match |= (((*entry) ^ (*search)) & (*mask));
                        }
                        found = (match == 0);
                        if ( (f->file_filter != NULL) && found ) {
                            // No need to call filter if not 'found'
                            handle= MAKE_RND_HANDLE( f->cur_pageno, f->page_record, f->file_index );
                            found = found && f->file_filter(handle);
                        }
                    }
                    // Position to next record for next search
                    f->page_record++;
                    f->cur_entry += width;
                }
            }
        } /* end while */
    } else {
        while (! found) {
            nw = c_waread2(iun, f->head_keys, f->cur_addr, width);
            header = (xdf_record_header *) f->head_keys;
            if ((header->idtyp >= 112) || (nw < width)) {
                if ((header->idtyp >= 112) && (header->idtyp < 127)) {
                    f->cur_addr += W64TOWD(1);
                }
                end_of_file = 1;
                break;
            }
            if (f->fstd_vintage_89) {
                /* old sequential standard */
                if ((stde = malloc(sizeof(stdf_dir_keys))) == NULL) {
                    Lib_Log(APP_LIBFST,APP_FATAL,"%s: memory is full\n",__func__);
                    return(ERR_MEM_FULL);
                }
                seq_entry = (seq_dir_keys *) f->head_keys;
                if (seq_entry->dltf) {
                    f->cur_addr += W64TOWD(((seq_entry->lng + 3) >> 2)+15);
                    continue;
                }
                if (seq_entry->eof > 0) {
                    header->idtyp = 112 + seq_entry->eof;
                    header->lng = 1;
                    end_of_file = 1;
                    break;
                }
                stde->deleted = 0;
                stde->select = 1;
                stde->lng = ((seq_entry->lng + 3) >> 2) + 15;
                stde->addr = (seq_entry->swa >> 2) +1;
                stde->deet = seq_entry->deet;
                stde->nbits = seq_entry->nbits;
                stde->ni = seq_entry->ni;
                stde->gtyp = seq_entry->grtyp;
                stde->nj = seq_entry->nj;
                stde->datyp = seq_entry->datyp;
                stde->nk = seq_entry->nk;
                stde->ubc = 0;
                stde->npas = (seq_entry->npas2 << 16) |
                seq_entry->npas1;
                stde->pad7 = 0;
                stde->ig4 = seq_entry->ig4;
                stde->ig2a = 0;
                stde->ig1 = seq_entry->ig1;
                stde->ig2b = seq_entry->ig2 >> 8;
                stde->ig3 = seq_entry->ig3;
                stde->ig2c = seq_entry->ig2 & 0xff;
                stde->etik15 =
                    (ascii6(seq_entry->etiq14 >> 24) << 24) |
                    (ascii6((seq_entry->etiq14 >> 16) & 0xff) << 18) |
                    (ascii6((seq_entry->etiq14 >>  8) & 0xff) << 12) |
                    (ascii6((seq_entry->etiq14      ) & 0xff) <<  6) |
                    (ascii6((seq_entry->etiq56 >>  8) & 0xff));
                stde->pad1 = 0;
                stde->etik6a =
                    (ascii6((seq_entry->etiq56      ) & 0xff) << 24) |
                    (ascii6((seq_entry->etiq78 >>  8) & 0xff) << 18) |
                    (ascii6((seq_entry->etiq78      ) & 0xff) << 12);
                stde->pad2 = 0;
                stde->etikbc = 0;
                stde->typvar = ascii6(seq_entry->typvar) << 6;
                stde->pad3 = 0;
                stde->nomvar =
                    (ascii6((seq_entry->nomvar >>  8) & 0xff) << 18) |
                    (ascii6((seq_entry->nomvar      ) & 0xff) << 12);
                stde->pad4 = 0;
                stde->ip1 = seq_entry->ip1;
                stde->levtyp = 0;
                stde->ip2 = seq_entry->ip2;
                stde->pad5 = 0;
                stde->ip3 = seq_entry->ip3;
                stde->pad6 = 0;
                stde->date_stamp = seq_entry->date;
                deet = stde->deet;
                npas = stde->npas;
                if (((deet*npas) % 3600) != 0) {
                    // Recompute datev to take care of rounding used with 1989 version de-octalise the date_stamp
                    run = stde->date_stamp & 0x7;
                    datexx = (stde->date_stamp >> 3) * 10 + run;

                    f_datev = (int32_t) datexx;
                    i_nhours = (deet*npas - ((deet*npas+1800)/3600)*3600);
                    nhours = (double) (i_nhours / 3600.0);
                    // PZ REPLACED: incdatr_(&f_datev, &f_datev, &nhours);
                    f_datev += nhours*3600;
                    // Re-octalise the date_stamp
                    datexx = (int) f_datev;
                    stde->date_stamp = 8 * (datexx/10) + (datexx % 10);
                }

                entry = (uint32_t *) stde;
                search = (uint32_t *) f->head_keys;
                for (i = 0; i < width; i++, entry++, search++) {
                    *search = *entry;
                }
                free(stde);
            } /* end if fstd_vintage_89 */

            if ((header->idtyp < 1) || (header->idtyp > 127)) {
                f->cur_addr += W64TOWD(header->lng);
                continue;
            }
            entry = (uint32_t *) f->head_keys;
            search = (uint32_t *) f->target;
            mask = (uint32_t *) f->cur_mask;
            match = 0;
            for (i = 0; i < width; i++, mask++, search++, entry++) {
                match |= (((*entry) ^ (*search)) & (*mask));
            }
            found = (match == 0);

            if (found) {
                f->valid_pos = 1;
                addr_match = f->cur_addr;
            }
            // Position to next record
            f->cur_addr += W64TOWD(header->lng);
            if (! f->fstd_vintage_89) {
                // Skip postfix
                f->cur_addr += W64TOWD(2);
            }
        }
    }

    if (! found) return ERR_NOT_FOUND;

    if (! f->xdf_seq) {
        Lib_Log(APP_LIBFST,APP_DEBUG,"%s: Record found at page# %d, record# %d\n",__func__,f->cur_pageno,f->page_record-1);
        handle = MAKE_RND_HANDLE(f->cur_pageno, f->page_record-1, f->file_index);
    } else {
        Lib_Log(APP_LIBFST,APP_DEBUG,"%s: Record found at address %d\n",__func__,addr_match);
        stde = (stdf_dir_keys *) f->head_keys;
        handle = make_seq_handle(addr_match, f->file_index, f);
    }
   return handle;
}


//! Find the position of the record as described by the given primary keys.
//
//! The search begins from the record pointed by handle.  If handle is 0,
//! the search is from beginning of file. If handle is -1, the search
//! begins from the current position. If a specific key has the value of
//! -1, this key will not used as a selection criteria.
//! If nprim is -1, the last selection criterias will be used for the
//! search. Upon completion a "pointer" to the record (handle) is returned.
//! \return Record handle if found or error code
int c_xdfloc2(
    //! [in] Unit number associated to the file
    int iun,
    //! [in] Handle to the starting search position
    int handle,
    //! [in] Primary search keys
    uint32_t *primk,
    //! [in] Number search primary search keys
    int nprim,
    //! [in] Search mask
    uint32_t *mskkeys
) {
    int i, record, pageno;
    int index, index_h, was_allocated, new_handle;
    uint32_t *pmask, *psmask;
    file_table_entry *f;
    xdf_record_header header;
    seq_dir_keys seq_entry;

    if ((index = file_index(iun)) == ERR_NO_FILE) {
        Lib_Log(APP_LIBFST,APP_ERROR,"%s: file is not open, iun=%d\n",__func__,iun);
        return(ERR_NO_FILE);
    }
    f = file_table[index];

    was_allocated = 0;
    if ((mskkeys == NULL) && (nprim != 0)) {
        mskkeys = malloc(nprim * sizeof(uint32_t));
        was_allocated = 1;
        for (i = 0; i < nprim; i++) {
            if (*primk == -1) {
                *mskkeys = 0;
            } else {
                *mskkeys = -1;
            }
        }
    }

    if (handle > 0) {
        // search begins from handle given position
        index_h = INDEX_FROM_HANDLE(handle);
        // Search from next record
        record = RECORD_FROM_HANDLE(handle) + 1;
        pageno = PAGENO_FROM_HANDLE(handle);
        if (index_h != index) {
            Lib_Log(APP_LIBFST,APP_ERROR,"%s: invalid handle=%d or iun=%d\n",__func__,handle,iun);
            return(ERR_BAD_HNDL);
         }
        if (f->xdf_seq) {
            f->cur_addr = address_from_handle(handle, f);
            if (f->fstd_vintage_89) {
                c_waread(iun, &seq_entry, f->cur_addr, sizeof(seq_entry) / sizeof(uint32_t));
                header.lng = ((seq_entry.lng + 3) >> 2) + 15;
            } else {
                c_waread(iun, &header, f->cur_addr, W64TOWD(1));
            }
            f->cur_addr += W64TOWD(header.lng);
        }
    } else if (handle == 0) {
        //! Search from beginning of file
        record = 0;
        pageno = 0;
        f->cur_pageno = -1;
        if (f->xdf_seq) {
            f->cur_addr = f->seq_bof;
        }
    } else if (handle == -1) {
        //! Search from current position
        if (((f->cur_entry == NULL) || (f->cur_pageno == -1)) && (! f->xdf_seq)) {
            Lib_Log(APP_LIBFST,APP_ERROR,"%s: current file position is invalid\n",__func__);
            return(ERR_NO_POS);
        }
    } else {
        Lib_Log(APP_LIBFST,APP_ERROR,"%s: invalid handle\n",__func__);
        return(ERR_BAD_HNDL);
    }

    // If nprim == 0 keep same search target
    if (nprim) {
        f->build_primary(f->target, primk, f->cur_mask, mskkeys, index, WMODE);
        pmask = (uint32_t *) f->cur_mask;
        psmask = (uint32_t *) f->srch_mask;
        for (i = 0; i < W64TOWD(f->primary_len); i++, pmask++, psmask++) {
            *pmask &= *psmask;
        }
        f->valid_target = 1;
    }

    if ((handle != -1) && (! f->xdf_seq)) {
        if (pageno != f->cur_pageno) {
            if (pageno < f->npages) {
                // Page is in current file
                f->cur_dir_page = f->dir_page[pageno];
                f->cur_pageno = pageno;
            } else {
                // Page is in a link file
                if (f->link == -1) {
                    Lib_Log(APP_LIBFST,APP_ERROR,"%s: page number=%d > last page=%d and file not linked\n",__func__,pageno,f->npages-1);
                    f->cur_entry = NULL;
                    return(ERR_BAD_PAGENO);
                }
                f->cur_dir_page = f->dir_page[f->npages - 1];
                f->cur_pageno = f->npages - 1;
                for (i = 0; (i <= pageno - f->cur_pageno) && (f->cur_dir_page); i++) {
                    f->cur_dir_page = (f->cur_dir_page)->next_page;
                    f->cur_pageno++;
                }
                if (f->cur_dir_page == NULL) {
                    Lib_Log(APP_LIBFST,APP_ERROR,"%s: invalid handle, invalid page number\n",__func__);
                    f->cur_entry = NULL;
                    return(ERR_BAD_PAGENO);
                }
            }
        } else {
            // Just to make sure
            f->cur_dir_page = f->dir_page[f->cur_pageno];
        }
        f->cur_entry = (f->cur_dir_page)->dir.entry + record * W64TOWD(f->primary_len);
        f->page_record = record;
    }

    if (((f->cur_entry == NULL) || (f->cur_pageno == -1)) && (! f->xdf_seq)) {
        Lib_Log(APP_LIBFST,APP_ERROR,"%s: no valid current file position\n",__func__);
        return(ERR_NO_POS);
    }

    if (! f->valid_target) {
        Lib_Log(APP_LIBFST,APP_ERROR,"%s: no valid current search target\n",__func__);
        return(ERR_NO_TARGET);
    }

    new_handle = next_match(index);
    if (was_allocated) free(mskkeys);
    return new_handle;
}

//! Find position of file iun in file table.
//! \return Index of the unit number in the file table or ERR_NO_FILE if not found
int file_index(
    //! [in] Unit number associated to the file
    const int iun
) {
    for (int i = 0; i < MAX_XDF_FILES; i++) {
        if (file_table[i] != NULL) {
            if (file_table[i]->iun == iun) {
                return i;
            }
        }
    }
    return ERR_NO_FILE;
}


//! Find index position in master file table (fnom file table).
//! \return Index of the provided unit number in the file table or -1 if not found.
int fnom_index(
    //! [in] Unit number associated to the file
    const int iun
) {
    for (int i = 0; i < MAXFILES; i++) {
        if (FGFDT[i].iun == iun) {
            return i;
        }
    }
    return -1;
}

//! Truncate a file
int c_secateur(
    //! Path of the file
    char *filePath,
    //! File size to set (in bytes)
    int size
) {
    Lib_Log(APP_LIBFST,APP_TRIVIAL,"%s: Truncating %s to \t %d Bytes\n",__func__,filePath,size);

    int ier = truncate(filePath, size);
    if (ier == -1) perror("secateur");
    return ier;
}



//! Initialize a file table entry.
static void init_file(
    //! [in] Index in the file table
    int index
) {
    int j;

    for (j = 1; j < MAX_DIR_PAGES; j++) {
        file_table[index]->dir_page[j] = NULL;
    }
    file_table[index]->cur_dir_page = NULL;
    file_table[index]->build_primary = NULL;
    file_table[index]->build_info = NULL;

    file_table[index]->scan_file = NULL;
    file_table[index]->file_filter = NULL;
    file_table[index]->cur_entry = NULL;
    if ((file_table[index]->file_index == index) && (file_table[index]->header != NULL)) {
        // Reuse file
        free(file_table[index]->header);
    }
    file_table[index]->header = NULL;
    file_table[index]->nxtadr = 1;
    file_table[index]->primary_len = 0;
    file_table[index]->info_len = 0;
    file_table[index]->link = -1;
    file_table[index]->iun = -1;
    file_table[index]->file_index = index;
    file_table[index]->modified = 0;
    file_table[index]->npages = 0;
    file_table[index]->nrecords = 0;
    file_table[index]->cur_pageno = -1;
    file_table[index]->page_record = 0;
    file_table[index]->page_nrecords = 0;
    file_table[index]->file_version = 0;
    file_table[index]->valid_target = 0;
    file_table[index]->xdf_seq = 0;
    file_table[index]->valid_pos = 0;
    file_table[index]->cur_addr = -1;
    file_table[index]->seq_bof = 1;
    file_table[index]->fstd_vintage_89 = 0;
    for (j = 0; j < MAX_SECONDARY_LNG; j++) {
        file_table[index]->info_keys[j] = 0;
    }
    for (j = 0; j < MAX_PRIMARY_LNG; j++) {
        file_table[index]->head_keys[j] = 0;
        file_table[index]->cur_keys[j] = 0;
        file_table[index]->target[j] = 0;
        file_table[index]->srch_mask[j] = -1;
        file_table[index]->cur_mask[j] = -1;
    }
}

//! Closes the XDF file. Rewrites file header, computes directory checksum and rewrites directory pages if modified.
int c_xdfcls(
    //! [in] Unit number associated to the file
    int iun
) {
    int index, index_fnom, i, j, lng64, width, open_mode;
    file_table_entry *f;
    xdf_dir_page *curpage;
    uint32_t *check32, checksum;
    uint32_t *entry;
    xdf_record_header *rec;

    index_fnom = fnom_index(iun);
    if (index_fnom == -1) {
        Lib_Log(APP_LIBFST,APP_ERROR,"%s: file is not connected with fnom\n",__func__);
        return(ERR_NO_FNOM);
    }

    if ((index = file_index(iun)) == ERR_NO_FILE) {
        Lib_Log(APP_LIBFST,APP_ERROR,"%s: file is not open\n",__func__);
        return(ERR_NO_FILE);
    }

    f = file_table[index];

    if ((f->header->rwflg != RDMODE) && (!FGFDT[index_fnom].attr.read_only)) {
        // Rewrite file header
        c_wawrit(iun, f->header, 1, W64TOWD(f->header->lng));
    }

    if (f->modified) {
        // File has been modified rewrite dir. pages
        for (i = 0; i < f->header->nbd; i++) {
            if (f->dir_page[i]->modified) {
                // Reset idtyp entries to original value if modified by scan_dir_page
                width = W64TOWD(f->primary_len);
                entry = (f->dir_page[i])->dir.entry;
                for (j = 0; j < (f->dir_page[i])->dir.nent; j++) {
                    rec = (xdf_record_header *) entry;
                    if ((rec->idtyp | 0x80) == 254) {
                        rec->idtyp = 255;
                        c_wawrit(iun, rec, W64TOWD(rec->addr - 1) + 1, W64TOWD(1));
                    }
                    rec->idtyp = ((rec->idtyp | 0x80) == 255) ? 255 : (rec->idtyp & 0x7f);
                    entry += width;
                }
                // Compute checksum and rewrite page
                lng64 = f->primary_len * ENTRIES_PER_PAGE + 4;
                curpage = &((f->dir_page[i])->dir);
                checksum = curpage->chksum;
                check32 = (uint32_t *) curpage;
                for (j = 4; j < W64TOWD(lng64); j++) {
                    checksum ^= check32[j];
                }
                curpage->chksum = checksum;
                c_wawrit(iun, curpage, W64TOWD(curpage->addr - 1) + 1, W64TOWD(lng64));
                f->dir_page[i]->modified = 0;
            } /* end if page modified */
         } /* end for i */
        if (f->xdf_seq) {
            {
                int trunc_to;
                trunc_to = FGFDT[index_fnom].file_size * sizeof(uint32_t);
                c_secateur(FGFDT[index_fnom].file_name, trunc_to);
            }
        }
        f->modified = 0;
    } // end if file modified

    if (!xdf_checkpoint) {
        if ((f->header->rwflg != RDMODE) && (!FGFDT[index_fnom].attr.read_only)) {
            // Rewrite file header
            f->header->rwflg = 0;
            c_wawrit(iun, f->header, 1, W64TOWD(f->header->lng));
        }

        c_waclos(iun);

        // Free allocated pages
        for (i = 0; i < f->npages; i++) {
            free(f->dir_page[i]);
        }

        // Reset file informations
        init_file(index);
    } else {
        xdf_checkpoint = 0;
    }

    return 0;
}




//! Add a directory page to file file_index
//! \return 0 on success.  Can be ERR_DIR_FULL or ERR_MEM_FULL in case of error.
static int32_t add_dir_page(
    //! [in] File index in file table
    int file_index,
    //! [in] Write flag. In case of a new file, or a file extenxion the directory page has to be written on disk
    int wflag
) {
    register file_table_entry *fte;
    int page_size, i, wdlng;
    full_dir_page *page;

    // check if file exists
    if( (fte = file_table[file_index]) == NULL) {
        return (ERR_NO_FILE);
    }

    // check if we can add a directory page, and if memory is available
    if(fte->npages >= MAX_DIR_PAGES) {
        Lib_Log(APP_LIBFST,APP_ERROR,"%s: Too many records, no more directory pages available\n",__func__);
        return(ERR_DIR_FULL);
    }
    // primary_len is given in unit of 64bits, hence the multiplication by 8
    page_size = sizeof(full_dir_page) + 8 * (fte->primary_len) * ENTRIES_PER_PAGE;

    // allocate directory page , link it into the directory page chain
    if ( (page = (page_ptr) malloc(page_size)) == NULL ) {
        return (ERR_MEM_FULL);
    }
    fte->dir_page[fte->npages] = page;
    (fte->dir_page[fte->npages])->next_page = NULL;
    if( fte->npages == 0) {
        // First page has no predecessor
        (fte->dir_page[fte->npages])->prev_page = NULL;
    } else {
        // Update succesor to preceding page
        (fte->dir_page[fte->npages])->prev_page = fte->dir_page[fte->npages-1];
        (fte->dir_page[fte->npages-1])->next_page = fte->dir_page[fte->npages];
    }

    // Initialize directory header and directory page entries
    page->modified = 0;
    page->true_file_index = file_index;
    page->dir.idtyp = 0;
    wdlng = ENTRIES_PER_PAGE *fte->primary_len + 4;
    page->dir.lng = wdlng;
    page->dir.addr = WDTO64(fte->nxtadr -1) +1;
    page->dir.reserved1 = 0;
    page->dir.reserved2 = 0;
    page->dir.nxt_addr = 0;
    page->dir.nent = 0;
    page->dir.chksum = 0;
    for (i = 0; i <= fte->primary_len*ENTRIES_PER_PAGE; i++) {
        page->dir.entry[i] = 0;
    }
    if (wflag) {
        // Write directory page to file
        if (fte->npages != 0) {
            // First page has no predecessor
            (fte->dir_page[fte->npages-1])->dir.nxt_addr = page->dir.addr;
        }
        c_wawrit(fte->iun, &page->dir, fte->nxtadr, W64TOWD(wdlng));
        fte->nxtadr += W64TOWD(wdlng);
        fte->header->fsiz = WDTO64(fte->nxtadr -1);
        fte->header->nbd++;
        fte->header->plst = page->dir.addr;
        // Checksum has to be computed
        page->modified = 1;
    }
    fte->npages++;
    return 0;
}


//! Write record (from buf) to xdf file.
//
//! If handle is not 0, rewrite record referenced by handle.
//! If handle is negative, the record will be append to end of file.
//! If handle is 0, the record is added at the end of file.
//! \return 0 on success, error code otherwise
int c_xdfput(
    //! [in] Unit number of the file to be written
    const int iun,
    //! [in] File index, page number and record number to record
    int handle,
    //! [in] Buffer to contain record
    buffer_interface_ptr buf)
{
    int index, record_number, page_number, i, idtyp, addr, lng, lngw;
    int index_from_buf, index_from_iun, write_to_end = 0, nwords;
    int write_addr, err, index_fnom, cluster_size, next_cluster_addr;
    file_table_entry *f, *f_buf;
    file_record *record, *bufrec;
    max_dir_keys primk, argument_not_used, mskkeys;
    xdf_record_header header64;
    burp_record *debug_record;
    postfix_seq postfix;

    index_fnom = fnom_index(iun);
    if ((index_from_buf = file_index(buf->iun)) == ERR_NO_FILE) {
        Lib_Log(APP_LIBFST,APP_ERROR,"%s: record not properly initialized\n",__func__);
        return(ERR_BAD_INIT);
    }

    if ((index_from_iun = file_index(iun)) == ERR_NO_FILE) {
        Lib_Log(APP_LIBFST,APP_ERROR,"%s: invalid iun (%d)\n",__func__,iun);
        return(ERR_BAD_UNIT);
    }

    if ((buf->nbits & 0x3f) != 0) {
        Lib_Log(APP_LIBFST,APP_ERROR,"%s: buf->nbits is not a multiple of 64 bits\n",__func__);
        return(ERR_BAD_ADDR);
    }

    nwords = buf->nbits / (8 * sizeof(uint32_t));
    index = index_from_iun;
    f = file_table[index_from_iun];

    if ((f->header->rwflg == RDMODE) || (FGFDT[index_fnom].attr.read_only)) {
        // Read only mode
        Lib_Log(APP_LIBFST,APP_ERROR,"%s: ile is open in read only mode or no write permission\n",__func__);
        return(ERR_NO_WRITE);
    }

    if ((handle != 0) && (f->header->rwflg == APPEND)) {
        Lib_Log(APP_LIBFST,APP_ERROR,"%s: file is open in append mode only\n",__func__);
        return(ERR_NO_WRITE);
    }

    if (handle <= 0) write_to_end = 1;

    if (handle != 0) {
        if (handle < 0 ) handle = -handle;

        index =         INDEX_FROM_HANDLE(handle);
        page_number =   PAGENO_FROM_HANDLE(handle);
        record_number = RECORD_FROM_HANDLE(handle);

        // Validate index, page number and record number
        if (index != index_from_iun) {
            Lib_Log(APP_LIBFST,APP_ERROR,"%s: iun and handle do not match\n",__func__);
            return(ERR_BAD_HNDL);
        }

        // Make sure that files are the same type
        if (index_from_iun != index_from_buf) {
            f_buf = file_table[index_from_buf];
            if ((f->header->nprm != f_buf->header->nprm) ||
                (f->header->lprm != f_buf->header->lprm) ||
                (f->header->naux != f_buf->header->naux) ||
                (f->header->laux != f_buf->header->laux)) {
                Lib_Log(APP_LIBFST,APP_ERROR,"%s: source and destination files are different type of file\n",__func__);
                return(ERR_NOT_COMP);
            }
        }
        if (! f->xdf_seq) {
            if (page_number < f->npages) {
                // Page is in current file
                f->cur_dir_page = f->dir_page[page_number];
            } else {
                // Page is in a link file
                if (f->link == -1) {
                    Lib_Log(APP_LIBFST,APP_ERROR,"%s: page number=%d > last page=%d and file not linked\n",__func__,page_number,f->npages-1);
                    return(ERR_BAD_PAGENO);
                }
                f->cur_dir_page = f->dir_page[f->npages-1];
                for (i = 0; ((i <= page_number - f->npages) && f->cur_dir_page); i++) {
                    f->cur_dir_page = (f->cur_dir_page)->next_page;
                }
                if (f->cur_dir_page == NULL) {
                    Lib_Log(APP_LIBFST,APP_ERROR,"%s: invalid handle, invalid page number\n",__func__);
                    return(ERR_BAD_PAGENO);
                }
            }

            if (record_number > f->cur_dir_page->dir.nent) {
                Lib_Log(APP_LIBFST,APP_ERROR,"%s: invalid handle, invalid record number\n",__func__);
                return(ERR_BAD_HNDL);
            }

            f->cur_entry = f->cur_dir_page->dir.entry + record_number * W64TOWD(f->primary_len);

            record = (file_record *) f->cur_entry;
        } else {
            // file is xdf sequential
            if (handle > 0) {
                // Rewrite record
                addr = address_from_handle(handle, f);
                c_waread(iun, f->head_keys, addr, MAX_PRIMARY_LNG);
                record = (file_record *) f->head_keys;
            }
        }
        idtyp = record->idtyp;
        addr = record->addr;
        lng = record->lng;
        lngw = W64TOWD(lng);

        if (idtyp == 0) {
            Lib_Log(APP_LIBFST,APP_ERROR,"%s: special record idtyp=0\n",__func__);
            return(ERR_SPECIAL);
        }

        if ((idtyp & 0x7E) == 0x7E) {
            Lib_Log(APP_LIBFST,APP_ERROR,"%s: deleted record\n",__func__);
            return(ERR_DELETED);
        }

        if (lngw != nwords) {
            /* enforce rewrite to end of file */
            write_to_end = 1;
        } else {
            write_addr = W64TOWD(addr-1)+1;
        }

    } // end if handle != 0

    if ((write_to_end) && (! f->xdf_seq)) {
        f->cur_dir_page = f->dir_page[f->npages - 1];
        f->cur_pageno = f->npages - 1;
        if (f->cur_dir_page->dir.nent >= ENTRIES_PER_PAGE) {
            f->cur_dir_page->modified = 1;
            err = add_dir_page(index, WMODE);
            if (err < 0) return err;
            f->cur_dir_page = f->dir_page[f->npages-1];
            f->cur_entry = f->cur_dir_page->dir.entry;
        } else {
            f->cur_entry = f->cur_dir_page->dir.entry + f->cur_dir_page->dir.nent * W64TOWD(f->primary_len);
        }
        f->page_nrecords = f->cur_dir_page->dir.nent++;
        write_addr = f->nxtadr;
    }

    if ((write_to_end) && (f->xdf_seq)) write_addr = f->cur_addr;

    if (handle != 0) {
        // Rewrite, delete old record
        err = c_xdfdel(handle);
        if (err < 0) return err;
    }


    // update record header
    bufrec = (file_record *) buf->data;
    bufrec->addr = WDTO64(write_addr - 1) + 1;
    if (f->xdf_seq) {
        next_cluster_addr = f->cur_addr -1 + nwords + W64TOwd(2);
        if ((next_cluster_addr >> 18) >= 512) {
            cluster_size = 128;
        } else if ((next_cluster_addr >> 18) >= 128) {
            cluster_size = 32;
        } else if ((next_cluster_addr >> 18) >= 32) {
            cluster_size = 8;
        } else {
            cluster_size = 2;
        }
        next_cluster_addr = ((next_cluster_addr + cluster_size - 1) / cluster_size) * cluster_size;
        nwords = next_cluster_addr - f->cur_addr - W64TOwd(2) + 1;
    }
    bufrec->lng = WDTO64(nwords);

    // Write record to file
    c_wawrit(iun, &(buf->data), write_addr, nwords);
    if (f->xdf_seq) {
        f->cur_addr += nwords;
    }

    if (! f->xdf_seq) {
        // Update directory entry
        debug_record = (burp_record *) f->cur_entry;
        // Get keys
        f->build_primary(buf->data, primk, argument_not_used, mskkeys, index, RDMODE);
        f->build_primary(f->cur_entry, primk, argument_not_used, mskkeys, index, WMODE);
        record = (file_record *) f->cur_entry;
        record->idtyp = bufrec->idtyp;
        record->addr = WDTO64(write_addr - 1) + 1;
        record->lng = WDTO64(nwords);
    }

    // Update file header
    f->header->nrec++;
    if (write_to_end) {
        f->header->nxtn++;
        f->header->fsiz += WDTO64(nwords);
        f->nxtadr = W64TOWD(f->header->fsiz) + 1;
        f->header->nbig = (WDTO64(nwords) > f->header->nbig) ? WDTO64(nwords) : f->header->nbig;
        if (f->xdf_seq) {
            // Add postfix and eof marker
            postfix.idtyp = 0;
            postfix.lng = 2;
            postfix.addr = -1;
            postfix.prev_idtyp = bufrec->idtyp;
            postfix.prev_lng = bufrec->lng;
            postfix.prev_addr = WDTO64(write_addr -1) + 1;
            c_wawrit(iun, &postfix, f->cur_addr, W64TOWD(2));
            f->cur_addr += W64TOWD(2);
            // 112 + 15, 15 means EOF
            header64.idtyp = 127;
            header64.lng = 1;
            header64.addr = WDTO64(f->cur_addr - 1) + 1;
            f->nxtadr = f->cur_addr;
            c_wawrit(iun, &header64, f->cur_addr, W64TOWD(1));
            FGFDT[index_fnom].file_size = f->nxtadr + W64TOWD(1) - 1;
        }
    }
    if (handle != 0) {
        f->header->nrwr++;
    }

    f->modified = 1;
    if (! f->xdf_seq) {
        f->cur_dir_page->modified = 1;
    }
    return 0;
}



//! Delete record referenced by handle.
//
//! Deleted record are marked as idtyp = X111111X (X = don't care bits)
//! and will be marked as idtyp=255 upon closing of the file.
int c_xdfdel(
    //! [in] File index, page number and record number to record
    const int handle
) {
    int index, page_number, record_number, idtyp, i, addr;
    file_table_entry *f;
    file_record *record;
    uint32_t *rec;
    page_ptr target_page;
    xdf_record_header header;

    index =         INDEX_FROM_HANDLE(handle);
    page_number =   PAGENO_FROM_HANDLE(handle);
    record_number = RECORD_FROM_HANDLE(handle);

    // validate index, page number and record number

    if ((index >= MAX_XDF_FILES) || (file_table[index] == NULL) || (file_table[index]->iun < 0)) {
        Lib_Log(APP_LIBFST,APP_ERROR,"%s: invalid handle, invalid file index\n",__func__);
        return(ERR_BAD_HNDL);
    }

    f = file_table[index];

    if ((f->header->rwflg == RDMODE) || (f->header->rwflg == APPEND)) {
        Lib_Log(APP_LIBFST,APP_ERROR,"%s: file is open in read or append mode only\n",__func__);
        return(ERR_RDONLY);
    }

    if (f->cur_info->attr.read_only) {
        Lib_Log(APP_LIBFST,APP_ERROR,"%s: file is read only\n",__func__);
        return(ERR_RDONLY);
    }

    if (! f->xdf_seq) {
        if (page_number < f->npages) {
            // Page is in current file
            target_page = f->dir_page[page_number];
        } else {
            // Page is in a link file
            if (f->link == -1) {
                Lib_Log(APP_LIBFST,APP_ERROR,"%s: page number=%d > last page=%d and file not linked\n",__func__,page_number,f->npages-1);
                return(ERR_BAD_PAGENO);
            }
            target_page = f->dir_page[f->npages-1];
            for (i = 0; (i <= (page_number - f->npages)) && target_page; i++) {
                target_page = (target_page)->next_page;
            }
            if (target_page == NULL) {
                Lib_Log(APP_LIBFST,APP_ERROR,"%s: invalid handle, invalid page number\n",__func__);
                return(ERR_BAD_PAGENO);
            }
        }

        if (record_number > target_page->dir.nent) {
            Lib_Log(APP_LIBFST,APP_ERROR,"%s: invalid handle, invalid record number\n",__func__);
            return(ERR_BAD_HNDL);
        }

        rec = target_page->dir.entry + record_number * W64TOWD(f->primary_len);
        record = (file_record *) rec;

        idtyp = record->idtyp;
    } else {
        // xdf sequential
        addr = address_from_handle(handle, f);
        c_waread(f->iun, &header, addr, W64TOWD(1));
        idtyp = header.idtyp;
    }

    if (idtyp == 0) {
        Lib_Log(APP_LIBFST,APP_WARNING,"%s: special record idtyp=0\n",__func__);
        return(ERR_SPECIAL);
    }

    if ((idtyp & 0x7E) == 0x7E) {
        Lib_Log(APP_LIBFST,APP_WARNING,"%s: record already deleted\n",__func__);
        return(ERR_DELETED);
    }

    if (! f->xdf_seq) {
        // update directory entry
        record->idtyp = 254;
        target_page->modified = 1;
    } else {
        // xdf sequential
        // deleted
        header.idtyp = 255;
        c_wawrit(f->iun, &header, addr, W64TOWD(1));
    }

    // Update file header
    f->header->neff++;
    f->header->nrec--;

    f->modified = 1;
    return 0;
}



//! Check XDF file for corruption
//! @return              Valid(0) or not(-1)
int c_xdfcheck(
    //! [in] filePath Path to the file
    const char *filePath
) {
    file_header header;
    uint32_t tmp;
    uint32_t *buf_ptr = (uint32_t *) &header;
    FILE *fd = fopen(filePath, "r");

    if (!fd) {
        Lib_Log(APP_LIBFST,APP_ERROR,"%s: Cannot open file\n",__func__);
        return(ERR_NO_FILE);
    }

    // Read the header
    int num_records = fread(buf_ptr, sizeof(header), 1, fd);

    // Get file size
    fseek(fd, 0L, SEEK_END);
    long file_size = ftell(fd);
    fseek(fd, 0L, SEEK_SET);

    fclose(fd);

    // Flip bytes in each 32-bit word (16 of them)
    for(int i = 0 ; i < 16 ; i++) {
        tmp = buf_ptr[i] ;
        buf_ptr[i] = (tmp << 24) | (tmp >> 24) | ((tmp & 0xFF0000) >> 8) | ((tmp & 0xFF00) << 8) ;
    }

    if ((size_t)header.fsiz * 8 != file_size) {
        Lib_Log(APP_LIBFST,APP_ERROR,"%s: File size does not match header. Expected size: %d bytes. Actual size: %d\n",__func__,header.fsiz*8,file_size);
        return(ERR_DAMAGED);
    }

    if (header.idtyp != 0) {
        Lib_Log(APP_LIBFST,APP_ERROR,"%s: rong header ID type (%d), should be %d\n",__func__,header.idtyp,0);
        return(ERR_DAMAGED);
    }

    return 0;
}


//! Find a free position in file table and initialize file attributes.
//! \return Free position index or error code
static int get_free_index()
{
    int nlimite;

    if (STDSEQ_opened == 1) {
        nlimite = 128;
    } else {
        nlimite = MAX_XDF_FILES;
    }
    for (int i = 0; i < nlimite; i++) {
        if (file_table[i] == NULL) {
            if ((file_table[i] = (file_table_entry_ptr) malloc(sizeof(file_table_entry))) == NULL) {
                 Lib_Log(APP_LIBFST,APP_FATAL,"%s: can't alocate file_table_entry\n",__func__);
                return(ERR_MEM_FULL);
            }
            // assure first time use of index i
            file_table[i]->file_index = -1;
            init_file(i);
            return i;
        } else {
            if (file_table[i]->iun == -1) {
                return i;
            }
         }
    }
    Lib_Log(APP_LIBFST,APP_FATAL,"%s: xdf file table is full\n",__func__);
    return(ERR_FTAB_FULL);
}


static int init_package_done = 0;

//! Initialize all file table entries
static void init_package()
{
    int i, ind;

    for (i = 0; i < MAX_XDF_FILES; i++) {
        file_table[i] = NULL;
    }
    // Init first entry to start with file index = 1
    ind = get_free_index();
    file_table[ind]->iun = 1234567;
    init_package_done = 1;
}


//! Pack fstd primary keys into buffer or get primary keys from buffer depending on mode argument.
void build_fstd_prim_keys(
    //! [inout] Buffer to contain the keys
    uint32_t *buf,
    //! [inout] Primary keys
    uint32_t *keys,
    //! [out] Search mask
    uint32_t *mask,
    //! [in] Unpacked masks
    uint32_t *mskkeys,
    //! [in] File index in file table
    int index,
    //! [in] If mode = WMODE, write to buffer otherwise get keys from buffer
    int mode
) {
    file_header *fh;
    int i, wi, sc, rmask, key, wfirst, wlast;

    // Skip first 64 bit header
    buf += W64TOWD(1);

    // First 64 bits not part of the search mask
    mask[0] = 0;
    if (W64TOWD(1) > 1) mask[1] = 0;

    // Skip first 64 bit header
    mask += W64TOWD(1);

    fh = file_table[index]->header;

    if (mode == WMODE) {
        // Erite keys to buffer
        for (i = 0; i < W64TOWD(fh->lprm -1); i++) {
            buf[i] = keys[i];
            mask[i] = mskkeys[i];
        }
    } else {
        for (i = 0; i < W64TOWD(fh->lprm -1); i++) {
            keys[i] = buf[i];
        }
    }
}


//! Pack generic info keys into buffer or get info keys from buffer depending on mode argument.
static void build_gen_info_keys(
    //! [inout] Buffer to contain the keys
    uint32_t *buf,
    //! [inout] Info keys
    uint32_t *keys,
    //! [in] File index in file table
    int index,
    //! [in] Ff mode = WMODE, write to buffer otherwise get keys from buffer
    int mode
) {
    //! \todo Shouldn't this comme from somewhere else?
    const int bitmot = 32;

    file_header * fh = file_table[index]->header;

    if (mode == WMODE) {
        // Write keys to buffer
        for (int i = 0; i < fh->naux; i++) {
            if (keys[i] != -1) {
                int wi = fh->keys[i+fh->nprm].bit1 / bitmot;
                int sc = (bitmot-1) - (fh->keys[i+fh->nprm].bit1 % bitmot);
                int rmask = -1 << (fh->keys[i+fh->nprm].lcle);
                rmask = ~(rmask << 1);
                // equivalent of << lcle+1 and covers 32 bit case
                int key = keys[i];
                if ((fh->keys[i+fh->nprm].tcle /32) > 0)
                key = key & (~((key & 0x40404040) >> 1));
                // Clear bits
                buf[wi] = buf[wi] & (~(rmask << sc));
                buf[wi] = buf[wi] | ((key & rmask) << sc);
            }
        }
    } else {
        for (int i = 0; i < fh->naux; i++) {
            int wi = fh->keys[i+fh->nprm].bit1 / bitmot;
            int sc = (bitmot-1) - (fh->keys[i+fh->nprm].bit1 % bitmot);
            int rmask = -1 << (fh->keys[i+fh->nprm].lcle);
            rmask = ~(rmask << 1);
            keys[i] = (buf[wi] >> sc) & rmask;
        }
    }
}


//! Pack generic primary keys into buffer or get primary keys from buffer depending on mode argument.
static void build_gen_prim_keys(
    //! [inout] Buffer to contain the keys
    uint32_t *buf,
    //! [inout] Primary keys
    uint32_t *keys,
    //! [out] Search mask
    uint32_t *mask,
    //! [in] Unpacked masks
    uint32_t *mskkeys,
    //! [in] File index in file table
    int index,
    //! [in] if this is set to WMODE, write to buffer otherwise get keys from buffer
    int mode
) {
    const int bitmot = 32;

    // Skip first 64 bit header
    buf += 2;

    // First 64 bits not part of the search mask
    mask[0] = 0;
    mask[1] = 0;

    // Skip first 64 bit header
    mask += 2;

    file_header * fh = file_table[index]->header;

    if (mode == WMODE) {
        // Write keys to buffer
        int wfirst = fh->keys[0].bit1 / bitmot;
        int wlast = fh->keys[fh->nprm-1].bit1 / bitmot;
        for (int i = wfirst; i <= wlast; i++) {
            mask[i] = 0;
        }

        for (int i = 0; i < fh->nprm; i++) {
            if (keys[i] != -1) {
                int wi = fh->keys[i].bit1 / bitmot;
                int sc = (bitmot-1) - (fh->keys[i].bit1 % bitmot);
                int rmask = -1 << (fh->keys[i].lcle);
                rmask = ~(rmask << 1);
                /* equivalent of << lcle+1 and covers 32 bit case */
                int key = keys[i];
                if ((fh->keys[i].tcle /32) > 0) {
                    key = key & (~((key & 0x40404040) >> 1));
                }
                // clear bits
                buf[wi] = buf[wi] & (~(rmask << sc));
                buf[wi] = buf[wi] | ((key & rmask) << sc);
                // clear bits
                mask[wi] = mask[wi] & (~(rmask << sc));
                mask[wi] = mask[wi] | (rmask << sc);
            }
        }
    } else {
        for (int i = 0; i < fh->nprm; i++) {
            int wi = fh->keys[i].bit1 / bitmot;
            int sc = (bitmot-1) - (fh->keys[i].bit1 % bitmot);
            int rmask = -1 << (fh->keys[i].lcle);
            rmask = ~(rmask << 1);
            keys[i] = (buf[wi] >> sc) & rmask;
        }
    }
}


//! Pack fstd info keys into buffer or get info keys from buffer depending on mode argument
//! @deprecated This function does absolutely nothing
void build_fstd_info_keys(
    //! [inout] Buffer to contain the keys
    uint32_t *buf,
    //! [inout] Info keys
    uint32_t *keys,
    //! [in] File index in file table
    int index,
    //! [in] if mode = WMODE, write to buffer otherwise get keys from buffer.
    int mode
) {
}



//! Create a new XDF file.
//! \return 0 on success, error code otherwise
static int create_new_xdf(
    //! [in] File index in table
    int index,
    //! [in] Unit number associated to the file
    int iun,
    //! [in] Primary keys
    word_2 *pri,
    //! [in] Number of primary keys
    int npri,
    //! [in] Auxiliary keys
    word_2 *aux,
    //! [in] Number of auxiliary keys
    int naux,
    //! [in] Application signature
    char *appl
) {
    // FIXME: No validation of user provided naux, npri befoe allocation
    // Why no allocate sizeof(file_header) for the header?
    // FIXME: According to the definition qstdir.h, the size of, file_table[index]->header, which is of type file_header, is static!

    file_header *file;
    int ikle = 0, lprm = 0, laux = 0;
    int lng_header = naux + npri + 512 / 64;

    if ((file_table[index]->header = malloc(lng_header * 8)) == NULL) {
        Lib_Log(APP_LIBFST,APP_FATAL,"%s: memory is full\n",__func__);
        return(ERR_MEM_FULL);
    }
    file = file_table[index]->header;
    file->vrsn = 'X' << 24 | 'D' << 16 | 'F' << 8 | '0';
    file->sign = appl[0] << 24 | appl[1] << 16 | appl[2] << 8 | appl[3];
    file->idtyp = 0;
    // keys + fixed part of 512 bits
    file->lng = lng_header;
    file->addr = 0;
    // keys + fixed part of 512 bits
    file->fsiz = lng_header;
    file->nrwr = 0;
    file->nxtn = 0;
    file->nbd = 0;
    file->plst = 0;
    file->nbig = 0;
    file->nprm = npri;
    file->naux = naux;
    file->neff = 0;
    file->nrec = 0;
    file->rwflg = 0;
    file->reserved = 0;

   {
        int i = 0 , bit1 = 0, lcle = 0, tcle = 0;
        while (npri--) {
            file->keys[ikle].ncle = pri[i].wd1;
            bit1 = pri[i].wd2 >> 19;
            lcle = 0x1F & (pri[i].wd2 >> 14);
            tcle = 0x3F  & (pri[i].wd2 >> 8);
            file->keys[ikle].bit1 = bit1;
            file->keys[ikle].lcle = lcle;
            file->keys[ikle].tcle = tcle;
            file->keys[ikle].reserved = 0;
            lprm += lcle;
            i++;
            ikle++;
        }
   }

   /* primary keys + 64 bit header */
   lprm = (lprm + 63) / 64 + 1;
   file->lprm = lprm;
   file_table[index]->primary_len = lprm;
   {
       int i=0 , bit1=0, lcle=0, tcle=0;
        while (naux--) {
            file->keys[ikle].ncle=aux[i].wd1;
            bit1=aux[i].wd2 >> 19;
            lcle=0x1F & (aux[i].wd2 >> 14);
            tcle=0x3F  & (aux[i].wd2 >> 8);
            file->keys[ikle].bit1=bit1;
            file->keys[ikle].lcle=lcle;
            file->keys[ikle].tcle=tcle;
            file->keys[ikle].reserved=0;
            laux += lcle;
            i++; ikle++;
        }
   }
   laux = (laux + 63) / 64;
   file->laux = laux;
   file_table[index]->info_len = laux;
   if (! file_table[index]->cur_info->attr.read_only) {
        {
            int unit = iun, waddress = 1, nwords = file->fsiz * 8 / sizeof(uint32_t);
            c_wawrit(unit, file_table[index]->header, waddress, nwords);
            file_table[index]->nxtadr += nwords;
        }
   }
   return 0;
}


//! Open an XDF file.
int c_xdfopn(
    //! [in] Unit number associated to the file
    int iun,
    //! [in] Open mode (READ, WRITE, R-W, CREATE, APPEND)
    char *mode,
    //! [in] Primary keys
    word_2 *pri,
    //! [in] Number of primary keys
    int npri,
    //! [in] Info keys
    word_2 *aux,
    //! [in] Number of info keys
    int naux,
    //! [in] Application signature
    char *appl
) {
    file_table_entry *f;
    int32_t f_datev;
    double nhours;
    int deet, npas, i_nhours, run, datexx;
    uint32_t STDR_sign = 'S' << 24 | 'T' << 16 | 'D' << 8 | 'R';
    uint32_t STDS_sign = 'S' << 24 | 'T' << 16 | 'D' << 8 | 'S';

    if (!init_package_done) {
        init_package();
        init_package_done = 1;
    }

    if ((iun <= 0) || (iun > 999)) {
        Lib_Log(APP_LIBFST,APP_ERROR,"%s: invalid unit number=%d\n",__func__,iun);
        return(ERR_BAD_UNIT);
    }

    if (file_index(iun) != ERR_NO_FILE) {
        Lib_Log(APP_LIBFST,APP_ERROR,"%s: file (unit=%d) is already open\n",__func__,iun);
        return(ERR_FILE_OPN);
    }

    int index = get_free_index();
    file_table[index]->iun = iun;
    file_table[index]->file_index = index;

    int index_fnom = fnom_index(iun);
    if (index_fnom == -1) {
        Lib_Log(APP_LIBFST,APP_ERROR,"%s: file (unit=%d) is not connected with fnom\n",__func__,iun);
        return(ERR_NO_FNOM);
    }

    f = file_table[index];
    f->cur_info = &FGFDT[index_fnom];

    if (! f->cur_info->attr.rnd) {
        Lib_Log(APP_LIBFST,APP_ERROR,"%s: file must be random\n file in error: %s\n",__func__,FGFDT[index_fnom].file_name);
        return(-1);
    }

    if (strstr(appl, "BRP0")) f->cur_info->attr.burp = 1;
    if (strstr(appl, "STD"))  f->cur_info->attr.std = 1;

    if (f->cur_info->attr.burp) {
        // PZ DISABLE: f->build_primary = (fn_b_p *) build_burp_prim_keys;
        // PZ DISABLE:  f->build_info = (fn_ptr *) build_burp_info_keys;
    } else {
        if (f->cur_info->attr.std) {
            f->build_primary = (fn_b_p *) build_fstd_prim_keys;
            f->build_info = (fn_ptr *) build_fstd_info_keys;
            // PZ DISABLE: f->file_filter = (fn_ptr *) C_fst_match_req;
        } else {
            f->build_primary = (fn_b_p *) build_gen_prim_keys;
            f->build_info = (fn_ptr *) build_gen_info_keys;
        }
    }

    if ((strstr(f->cur_info->file_type, "SEQ")) || (strstr(f->cur_info->file_type, "seq"))) {
        f->xdf_seq = 1;
        // at least one seq file is opened, limit number of xdf files is now 128
        STDSEQ_opened = 1;
        if (index > 127) {
            Lib_Log(APP_LIBFST,APP_ERROR,"%s: hile opening std/seq file, limit of 128 opened file reached\n",__func__);
            return(-1);
        }
    }

    Lib_Log(APP_LIBFST,APP_DEBUG,"%s: c_xdfopn f->xdf_seq=%d\n",__func__,f->xdf_seq);

    if (strstr(mode, "CREATE") || strstr(mode, "create")) {

        // Create new xdf file
        c_waopen(iun);
        int ier = create_new_xdf(index, iun, pri, npri, aux, naux, appl);
        if (! f->xdf_seq) {
            add_dir_page(index, WMODE);
        } else {
            f->cur_addr = f->nxtadr;
            f->header->fsiz = WDTO64(f->nxtadr -1);
        }
        f->header->rwflg = CREATE;
        f->modified = 1;
        return ier;
    }

    // File exist, read directory pages
    if (! FGFDT[index_fnom].open_flag) {
        c_waopen(iun);
    }

    int nrec = 0;
    {
        int wdaddress = 1;
        file_record header64;
        uint32_t *check32, checksum;
        int header_seq[30];
        int lng;
        stdf_struct_RND header_rnd;
        rnd_dir_keys *directory;
        stdf_dir_keys *stdf_entry;
        xdf_dir_page *curpage;

        c_waread(iun, &header64, wdaddress, W64TOWD(2));
        if (header64.data[0] == 'XDF0' || header64.data[0] == 'xdf0') {
            if ((f->header = calloc(1,header64.lng * 8)) == NULL) {
                Lib_Log(APP_LIBFST,APP_FATAL,"%s: memory is full\n",__func__);
                return(ERR_MEM_FULL);
            }

            // Read file header
            int wdlng_header = W64TOWD(header64.lng);
            c_waread(iun, f->header, wdaddress, wdlng_header);
            f->primary_len = f->header->lprm;
            f->info_len = f->header->laux;
            // nxtadr = fsiz +1
            f->nxtadr = W64TOWD(f->header->fsiz) + 1;
            wdaddress += wdlng_header;
            if (f->cur_info->attr.std) {
                if ((f->header->sign != STDR_sign) && (f->header->sign != STDS_sign)) {
                    Lib_Log(APP_LIBFST,APP_FATAL,"%s: %s is not a standard file\n",__func__,FGFDT[index_fnom].file_name);
                    return(ERR_WRONG_FTYPE);
                }
            }
            if (strstr(mode, "READ") || strstr(mode, "read")) {
                f->header->rwflg = RDMODE;
            } else {
                if (f->header->rwflg != RDMODE) {
                    Lib_Log(APP_LIBFST,APP_FATAL,"%s: file (unit=%d) currently used by another application in write mode\n",__func__,iun);
                    return(ERR_STILL_OPN);
                }

                if (strstr(mode, "WRITE") || strstr(mode, "write")) {
                    f->header->rwflg = WMODE;
                } else if (strstr(mode, "R-W") || strstr(mode, "r-w")) {
                    f->header->rwflg = RWMODE;
                } else if (strstr(mode, "APPEND") || strstr(mode, "append")) {
                    f->header->rwflg = APPEND;
                }
            }

            if (f->header->nbd == 0) {
                if ( (f->cur_info->attr.std) && (header64.data[1] == 'STDR' || header64.data[1] == 'stdr') ) {
                    Lib_Log(APP_LIBFST,APP_ERROR,"%s: File probably damaged\n file in error %s\n",__func__,FGFDT[index_fnom].file_name);
                    return( ERR_BAD_DIR);
                } else {
                    f->xdf_seq = 1;
                }
            } else {
                if (f->xdf_seq == 1) {
                    // Random file opened in seqential mode
                    f->header->rwflg = RDMODE;
                }
            }

            Lib_Log(APP_LIBFST,APP_DEBUG,"%s: fichier existe f->xdf_seq=%d\n",__func__,f->xdf_seq);

            if (! f->xdf_seq) {
                // Read directory pages and compute checksum
                for (int i = 0; i < f->header->nbd; i++) {
                    add_dir_page(index, RDMODE);
                    int lng64 = f->primary_len * ENTRIES_PER_PAGE + 4;
                    curpage = &((f->dir_page[f->npages-1])->dir);
                    c_waread(iun, curpage, wdaddress, W64TOWD(lng64));
                    checksum = 0;
                    check32 = (uint32_t *) curpage;
                    for (int j = 4; j < W64TOWD(lng64); j++) {
                        checksum ^= check32[j];
                    }
                    if (checksum != 0) {
                        Lib_Log(APP_LIBFST,APP_ERROR,"%s: incorrect checksum in page %d, directory is probably damaged\n file in error %s\n",__func__,i,FGFDT[index_fnom].file_name);
                    }
                    wdaddress = W64TOWD(curpage->nxt_addr - 1) +1;
                    if (((wdaddress == 0) && (i != f->header->nbd-1)) || (wdaddress > FGFDT[index_fnom].file_size)) {
                        Lib_Log(APP_LIBFST,APP_ERROR,"%s: number of directory pages is incorrect\n file in error %s\n",__func__,FGFDT[index_fnom].file_name);
                        return(ERR_BAD_DIR);
                    }
                    nrec += curpage->nent;
                } /* end for */

                f->nrecords = nrec;
            } else {
                // File is xdf sequential, position address to first record
                f->cur_addr = wdaddress;
                f->seq_bof = wdaddress;
            }
        } else {
            // Signature != XDF0
            check32 = (uint32_t *) &header64;
            if (*check32 == STDF_RND_SIGN) {
                // Old random standard file
                f->cur_info->attr.read_only = 1;
                f->fstd_vintage_89 = 1;
                lng = sizeof(header_rnd) / sizeof(uint32_t);
                c_waread(iun, &header_rnd, wdaddress, lng);
                wdaddress += lng;
                if ((directory = calloc(header_rnd.nutil, sizeof(uint32_t) * sizeof(rnd_dir_keys))) == NULL) {
                    Lib_Log(APP_LIBFST,APP_FATAL,"%s: memory is full\n",__func__);
                    return(ERR_MEM_FULL);
                }
                lng = header_rnd.nutil * sizeof(rnd_dir_keys) / sizeof(uint32_t);
                c_waread(iun, directory, wdaddress, lng);
                create_new_xdf(index, iun, (word_2 *)&stdfkeys, 16, aux, 0, "STDF");
                add_dir_page(index, RDMODE);
                f->cur_dir_page = f->dir_page[f->npages-1];
                for (int i = 0; i < header_rnd.nutil; i++) {
                    if (f->cur_dir_page->dir.nent >= ENTRIES_PER_PAGE) {
                        f->nrecords += f->page_nrecords;
                        add_dir_page(index, RDMODE);
                        f->cur_dir_page = f->dir_page[f->npages-1];
                    }
                    f->cur_entry = f->cur_dir_page->dir.entry + f->cur_dir_page->dir.nent * W64TOWD(f->primary_len);
                    stdf_entry = (stdf_dir_keys *) f->cur_entry;
                    f->page_nrecords = ++f->cur_dir_page->dir.nent;
                    if (! directory[i].dltf) {
                        stdf_entry->deleted = 0;
                        stdf_entry->select = 1;
                        stdf_entry->lng = (directory[i].lng + 3) >> 2;
                        stdf_entry->addr = (directory[i].swa >> 2) +1;
                        stdf_entry->deet = directory[i].deet;
                        stdf_entry->nbits = directory[i].nbits;
                        stdf_entry->ni = directory[i].ni;
                        stdf_entry->gtyp = directory[i].grtyp;
                        stdf_entry->nj = directory[i].nj;
                        stdf_entry->datyp = directory[i].datyp;
                        stdf_entry->nk = directory[i].nk;
                        stdf_entry->ubc = 0;
                        stdf_entry->npas = (directory[i].npas2 << 16) | directory[i].npas1;
                        stdf_entry->pad7 = 0;
                        stdf_entry->ig4 = directory[i].ig4;
                        stdf_entry->ig2a = 0;
                        stdf_entry->ig1 = directory[i].ig1;
                        stdf_entry->ig2b = directory[i].ig2 >> 8;
                        stdf_entry->ig3 = directory[i].ig3;
                        stdf_entry->ig2c = directory[i].ig2 & 0xff;
                        stdf_entry->etik15 =
                            (ascii6(directory[i].etiq14 >> 24) << 24) |
                            (ascii6((directory[i].etiq14 >> 16) & 0xff) << 18) |
                            (ascii6((directory[i].etiq14 >>  8) & 0xff) << 12) |
                            (ascii6((directory[i].etiq14      ) & 0xff) <<  6) |
                            (ascii6((directory[i].etiq56 >>  8) & 0xff));
                        stdf_entry->pad1 = 0;
                        stdf_entry->etik6a =
                            (ascii6((directory[i].etiq56      ) & 0xff) << 24) |
                            (ascii6((directory[i].etiq78 >>  8) & 0xff) << 18) |
                            (ascii6((directory[i].etiq78      ) & 0xff) << 12);
                        stdf_entry->pad2 = 0;
                        stdf_entry->etikbc = 0;
                        stdf_entry->typvar = ascii6(directory[i].typvar) << 6;
                        stdf_entry->pad3 = 0;
                        stdf_entry->nomvar =
                            (ascii6((directory[i].nomvar >>  8) & 0xff) << 18) |
                            (ascii6((directory[i].nomvar      ) & 0xff) << 12);
                        stdf_entry->pad4 = 0;
                        stdf_entry->ip1 = directory[i].ip1;
                        stdf_entry->levtyp = 0;
                        stdf_entry->ip2 = directory[i].ip2;
                        stdf_entry->pad5 = 0;
                        stdf_entry->ip3 = directory[i].ip3;
                        stdf_entry->pad6 = 0;
                        stdf_entry->date_stamp = directory[i].date;
                        deet = stdf_entry->deet;
                        npas = stdf_entry->npas;
                        if (((deet*npas) % 3600) != 0) {
                            // Recompute datev to take care of rounding used with 1989 version
                            // de-octalise the date_stamp
                            run = stdf_entry->date_stamp & 0x7;
                            datexx = (stdf_entry->date_stamp >> 3) * 10 + run;

                            f_datev = (int32_t) datexx;
                            i_nhours = (deet*npas - ((deet*npas+1800)/3600)*3600);
                            nhours = (double) (i_nhours / 3600.0);
                            // PZ REPLACED: incdatr_(&f_datev, &f_datev, &nhours);
                            f_datev += nhours*3600;
                            datexx = (int) f_datev;
                            // re-octalise the date_stamp
                            stdf_entry->date_stamp = 8 * (datexx/10) + (datexx % 10);
                        }
                    } else {
                        stdf_entry->deleted = 1;
                    }
                } /* end for */
                f->nrecords += f->page_nrecords;
                free(directory);
            } else {
                // Sequential
                c_waread(iun, &header_seq, 1, 30);
                if (header_seq[896/32] == STDF_SEQ_SIGN) {
                    // old sequential stdf
                    f->cur_info->attr.read_only = 1;
                    f->fstd_vintage_89 = 1;
                    f->xdf_seq = 1;
                    create_new_xdf(index, iun, (word_2 *)&stdfkeys, 16, aux, 0, "STDF");
                    f->cur_addr = 1;
                    f->seq_bof = 1;
                    return 0;
                } else {
                    Lib_Log(APP_LIBFST,APP_FATAL,"%s: file is not XDF type or old standard random type\n",__func__);
                    return(ERR_NOT_XDF);
                }
            }
        }
    }
    return f->header->nrec;
}



//! Obtain record referenced by handle in buf
int c_xdfget2(
    //! [in] File index, page number and record number to record
    const int handle,
    // [out] Buffer to contain record
    buffer_interface_ptr buf,
    int * const aux_ptr
) {
    int index, record_number, page_number, i, idtyp, addr, lng, lngw;
    int offset, nw, nread;
    file_table_entry *f;
    file_record *record;
    uint32_t *rec;
    max_dir_keys argument_not_used;
    page_ptr target_page;

    index =         INDEX_FROM_HANDLE(handle);
    page_number =   PAGENO_FROM_HANDLE(handle);
    record_number = RECORD_FROM_HANDLE(handle);

    // validate index, page number and record number
    if ((index >= MAX_XDF_FILES) || (file_table[index] == NULL) || (file_table[index]->iun < 0)) {
        Lib_Log(APP_LIBFST,APP_ERROR,"%s: invalid handle, invalid file index\n",__func__);
        return(ERR_BAD_HNDL);
    }

    f = file_table[index];

    if (! f->xdf_seq) {
        // Page is in current file
        if (page_number < f->npages) {
            target_page = f->dir_page[page_number];
        } else {
            // page is in a link file
            if (f->link == -1) {
                Lib_Log(APP_LIBFST,APP_ERROR,"%s: page number=%d > last page=%d and file not linked\n",__func__,page_number,f->npages-1);
                return(ERR_BAD_PAGENO);
            }
            target_page = f->dir_page[f->npages-1];
            for (i = 0; (i <= (page_number - f->npages)) && target_page; i++) {
                target_page = (target_page)->next_page;
            }
            if (target_page == NULL) {
                Lib_Log(APP_LIBFST,APP_ERROR,"%s: invalid handle, invalid page number\n",__func__);
                return(ERR_BAD_PAGENO);
            }
            f = file_table[target_page->true_file_index];
        }

        if (record_number > target_page->dir.nent) {
            Lib_Log(APP_LIBFST,APP_ERROR,"%s: invalid handle, invalid record number\n",__func__);
            return(ERR_BAD_HNDL);
        }

        rec = target_page->dir.entry + record_number * W64TOWD(f->primary_len);
        record = (file_record *) rec;
    } else {
        if (! f->valid_pos) {
            Lib_Log(APP_LIBFST,APP_ERROR,"%s: no valid file position for sequential file\n",__func__);
            return(ERR_NO_POS);
        }
        record = (file_record *) f->head_keys;
        if (address_from_handle(handle, f) != W64TOWD(record->addr - 1) + 1) {
            Lib_Log(APP_LIBFST,APP_ERROR,"%s: invalid handle, invalid address=%d record address=%d\n",__func__,address_from_handle(handle,f),W64TOWD(record->addr-1)+1);
            return(ERR_BAD_HNDL);
        }
    }

    idtyp = record->idtyp;
    addr = record->addr;
    lng = record->lng;
    lngw = W64TOWD(lng);

    if (idtyp == 0) {
        Lib_Log(APP_LIBFST,APP_WARNING,"%s: special record idtyp=0\n",__func__);
        return(ERR_SPECIAL);
    }

    if ((idtyp & 0x7E) == 0x7E) {
        Lib_Log(APP_LIBFST,APP_WARNING,"%s: deleted record\n",__func__);
        return(ERR_DELETED);
    }

    nw = buf->nwords;
    offset = 0;
    if (nw < 0) {
        if (buf->nbits != -1) {
            Lib_Log(APP_LIBFST,APP_ERROR,"%s: dimension of buf is invalid = %d\n",__func__,nw);
            return(ERR_BAD_DIM);
        }
        // data only, no directory entry in record
        nw = -nw;
        if (! f->fstd_vintage_89) {
            offset = W64TOWD(f->primary_len + f->info_len);
        } else {
            if (f->xdf_seq) {
                // old standard sequential
                offset = 30;
            }
        }
    }
    if (lngw > (nw - RECADDR +1)) {
        Lib_Log(APP_LIBFST,APP_ERROR,"%s: dimension of buf (%d) < record size (%d)\n",__func__,nw,lngw);
        return(ERR_BAD_DIM);
    }

    buf->nbits = lngw * 8 * sizeof(uint32_t);
    buf->record_index = RECADDR;
    buf->data_index = buf->record_index + W64TOWD(f->primary_len + f->info_len);
    buf->iun = f->iun;
    buf->aux_index = buf->record_index + W64TOWD(f->primary_len);

    if (aux_ptr != NULL) {
        *aux_ptr = 0;
        *(aux_ptr + 1) = 0;
    }
    if ( (aux_ptr != NULL) && (!f->fstd_vintage_89) && (!f->xdf_seq) ) {
        c_waread(buf->iun, aux_ptr, W64TOWD(addr-1) + 1 + W64TOWD(f->primary_len), W64TOWD(f->info_len));
    }

    for(i = 0; i < lngw; i++) {
        buf->data[i] = 0;
    }

    nread = c_waread2(buf->iun, &(buf->data), W64TOWD(addr - 1) + 1 + offset, lngw - offset);
    if (nread != lngw-offset) {
        Lib_Log(APP_LIBFST,APP_ERROR,"%s: short read, truncated record, asking for %d, got %d\n",__func__,lngw-offset,nread);
        return(ERR_SHORT_READ);
    }

    return 0;
}



//! Get the descriptive parameters of the record pointed by handle
//! \return 0 on success, error code otherwise
int c_xdfprm(
    //! [in] Buffer that contains the record
    int handle,
    //! [out] Record starting address
    int *addr,
    //! [out] Length of the record in 64 bit units
    int *lng,
    //! [out] Record id type
    int *idtyp,
    //! [out] Primary keys
    uint32_t *primk,
    //! [in] Number of primary keys
    int nprim
) {
    int index, record_number, page_number, i;
    file_table_entry *fte;
    file_record *record;
    uint32_t *rec;
    max_dir_keys argument_not_used;
    uint32_t *mskkeys = NULL;
    page_ptr target_page;

    index =         INDEX_FROM_HANDLE(handle);
    page_number =   PAGENO_FROM_HANDLE(handle);
    record_number = RECORD_FROM_HANDLE(handle);

    // Validate index, page number and record number
    if ((file_table[index] == NULL) || (file_table[index]->iun < 0)) {
        Lib_Log(APP_LIBFST,APP_ERROR,"%s: invalid handle, invalid file index\n",__func__);
        return(ERR_BAD_HNDL);
    }

    fte = file_table[index];

    if (! fte->xdf_seq) {
        if (page_number < fte->npages) {
            // Page is in current file
            target_page = fte->dir_page[page_number];
        } else {
            // Page is in a link file
            if (fte->link == -1) {
                Lib_Log(APP_LIBFST,APP_ERROR,"%s: page number=%d > last page=%d and file not linked\n",__func__,page_number,fte->npages-1);
                return(ERR_BAD_PAGENO);
            }
            target_page = fte->dir_page[fte->npages-1];
            for (i = 0; (i <= (page_number - fte->npages)) && target_page; i++) {
                target_page = (target_page)->next_page;
            }
            if (target_page == NULL) {
                Lib_Log(APP_LIBFST,APP_ERROR,"%s: invalid handle, invalid page number\n",__func__);
                return(ERR_BAD_PAGENO);
            }
        }

        if (record_number > target_page->dir.nent) {
            Lib_Log(APP_LIBFST,APP_ERROR,"%s: invalid handle, invalid record number\n",__func__);
            return(ERR_BAD_HNDL);
        }

        rec = target_page->dir.entry + record_number * W64TOWD(fte->primary_len);
        record = (file_record *) rec;
    } else {
        if (! fte->valid_pos) {
            Lib_Log(APP_LIBFST,APP_ERROR,"%s: no valid file position for sequential file\n",__func__);
            return(ERR_NO_POS);
        }
        record = (file_record *) fte->head_keys;
        if (address_from_handle(handle, fte) != W64TOWD(record->addr - 1) + 1) {
            Lib_Log(APP_LIBFST,APP_ERROR,"%s: invalid handle=%d, invalid address=%d record address=%d\n",__func__,handle,address_from_handle(handle,fte),W64TOWD(record->addr-1)+1);
            return(ERR_BAD_HNDL);
        }
    }

    *idtyp = record->idtyp;
    *addr = record->addr;
    *lng = record->lng;

    fte->build_primary((uint32_t *) record, primk, argument_not_used, mskkeys, index, RDMODE);

    return 0;
}


//! Links the list of random files together for record search purpose.
//! \return 0 on success, error code otherwise
int c_xdflnk(
    //! List of files indexes for the files to be linked
    int *liste,
    //! Number of files to be linked
    int n
) {
    int index, indnext, i, index_fnom;
    file_table_entry *f, *fnext;

    index_fnom = fnom_index(liste[0]);
    if (index_fnom == -1) {
        Lib_Log(APP_LIBFST,APP_ERROR,"%s: file is not connected with fnom\n",__func__);
        return(ERR_NO_FNOM);
    }

    if ((index = file_index(liste[0])) == ERR_NO_FILE) {
        Lib_Log(APP_LIBFST,APP_ERROR,"%s: file is not open\n",__func__);
        return(ERR_NO_FILE);
    }

    f = file_table[index];
    for (i=1; i < n; i++) {
        if ((index_fnom = fnom_index(liste[i])) == -1) {
           Lib_Log(APP_LIBFST,APP_ERROR,"%s: file is not connected with fnom\n",__func__);
           return(ERR_NO_FNOM);
        }
        if ((indnext = file_index(liste[i])) == ERR_NO_FILE) {
           Lib_Log(APP_LIBFST,APP_ERROR,"%s: file is not open\n",__func__);
           return(ERR_NO_FILE);
        }
        Lib_Log(APP_LIBFST,APP_DEBUG,"%s: xdflink %d avec %d\n",__func__,liste[i-1],liste[i]);

        fnext = file_table[indnext];
        f->link = indnext;
        (f->dir_page[f->npages-1])->next_page = fnext->dir_page[0];
        index = indnext;
        f = file_table[index];
    }

    return 0;
}


//! Unlinks the list of random files previously linked by c_xdflnk.
//! \return 0 on success, error code otherwise
int c_xdfunl(
    //! [in] Unit number associated to the file
    int *liste,
    //! [in] Number of files to be unlinked
    int n
) {
    int index, index_fnom, i;
    file_table_entry *fte;

    for (i = 0; i < n; i++) {
        if ((index_fnom = fnom_index(liste[i])) == -1) {
            Lib_Log(APP_LIBFST,APP_ERROR,"%s: file is not connected with fnom\n",__func__);
            return(ERR_NO_FNOM);
        }
        if ((index = file_index(liste[i])) == ERR_NO_FILE) {
            Lib_Log(APP_LIBFST,APP_ERROR,"%s: file is not open\n",__func__);
            return(ERR_NO_FILE);
        }
        fte = file_table[index];
        fte->link = -1;
        (fte->dir_page[fte->npages-1])->next_page = NULL;
    }

    return 0;
}

