/**
 * submit_bio_wait - submit a bio, and wait until it completes
 * @bio: The &struct bio which describes the I/O
 *
 * Simple wrapper around submit_bio(). Returns 0 on success, or the error from
 * bio_endio() on failure.
 *
 * WARNING: Unlike to how submit_bio() is usually used, this function does not
 * result in bio reference to be consumed. The caller must drop the reference
 * on his own.
 */
int submit_bio_wait(struct bio *bio)
{
        DECLARE_COMPLETION_ONSTACK(done);

        bio->bi_private = &done;
        bio->bi_end_io = submit_bio_wait_endio;
        bio->bi_opf |= REQ_SYNC;
        submit_bio(bio);
        wait_for_completion_io(&done);

        return blk_status_to_errno(bio->bi_status);
}
EXPORT_SYMBOL(submit_bio_wait);