#include <psm.h>
#include <psm_mq.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

char	 host[512];
uint64_t ch_size = 16;
int      ch_unit = 4;  // n bytes units
void     *local_base = NULL;
void     *remote_base = NULL;

typedef struct ralloc {
  uint8_t *base;
  uint32_t blks;
  uint64_t size;
  uint32_t next;
  uint32_t free;
  int unit;
} ralloc_t;

typedef struct psm_net_ch {
  psm_ep_t      ep;
  psm_mq_t      mq;
  psm_epid_t    epid;
  psm_epaddr_t *eps;  // array of endpoint addresses indexed from 0..n
  ralloc_t *l_allocator;
  ralloc_t *r_allocator;
  int rank_self;
  int rank_peer;
} psm_net_ch_t;

int try_to_initialize_psm(psm_net_ch_t *ch, psm_uuid_t job_uuid) {
  int verno_major = PSM_VERNO_MAJOR;
  int verno_minor = PSM_VERNO_MINOR;

  int err =
      psm_error_register_handler(NULL,			      // Global handler
				 PSM_ERRHANDLER_NO_HANDLER);  // return errors
  if (err) {
    fprintf(stderr, "Couldn't register global handler: %s\n",
	    psm_error_get_string(err));
    return -1;
  }

  err = psm_init(&verno_major, &verno_minor);
  if (err || verno_major > PSM_VERNO_MAJOR) {
    if (err)
      fprintf(stderr, "PSM initialization failure: %s\n",
	      psm_error_get_string(err));
    else
      fprintf(stderr,
	      "PSM loaded an unexpected/unsupported "
	      "version (%d.%d)\n",
	      verno_major, verno_minor);
    return -1;
  }

  // We were able to initialize PSM but will defer all further error
  // handling since most of the errors beyond this point will be fatal.
  err = psm_error_register_handler(NULL,  // Global handler
				   PSM_ERRHANDLER_PSM_HANDLER);  //
  if (err) {
    fprintf(stderr, "Couldn't register global errhandler: %s\n",
	    psm_error_get_string(err));
    return -1;
  }
  /*psm_ep_t ep;*/
  /*psm_epid_t epid;*/
  /*psm_uuid_t job_uuid;*/

  struct psm_ep_open_opts epopts;
  // Let PSM assign its default values to the endpoint options.
  psm_ep_open_opts_get_defaults(&epopts);

  // We want a stricter timeout and a specific unit
  epopts.timeout = 151e9;   // 15 second timeout
  epopts.unit    = -1;      // We want a specific unit, -1 would let PSM
			    // choose the unit for us.
  epopts.port	 = 0;  // We want a specific unit, <= 0 would let PSM
  epopts.network_pkey = PSM_EP_OPEN_PKEY_DEFAULT;
  // choose the port for us.
  // We've already set affinity, don't let PSM do so if it wants to.
  if (epopts.affinity == PSM_EP_OPEN_AFFINITY_SET)
    epopts.affinity = PSM_EP_OPEN_AFFINITY_SKIP;

  // job_uuid[0] = 0x1;
  if ((err = psm_ep_open(job_uuid, &epopts, &ch->ep, &ch->epid)) != PSM_OK) {
    fprintf(stderr, "psm_ep_open failed with error %s\n",
	    psm_error_get_string(err));
    // MPIU_ERR_SETANDJUMP(mpi_errno, MPI_ERR_OTHER, "**psmepopen");
    return -1;
  }

  if ((err = psm_mq_init(ch->ep, PSM_MQ_ORDERMASK_NONE, NULL, 0,  &ch->mq)) != PSM_OK) {
    fprintf(stderr, "psm_mq_init failed with error %s\n",
	    psm_error_get_string(err));
    // MPIU_ERR_SETANDJUMP(mpi_errno, MPI_ERR_OTHER, "**psmepopen");
    return -1;
  }
   
  return err;
}

#define MAX_EP_ADDR 2

// statically initialize epids
// assume we have 2 EPIDs
// which we figured out somehow
// generic bootstrap was not used here
//  dagger01 --> 0xd0203
//  dagger02 --> 0xf0203
psm_epid_t g_epids[MAX_EP_ADDR] = {0xd0203, 0xf0203};
int g_epmask[MAX_EP_ADDR] = {1, 1};

static int get_num_epids_known(){
  return sizeof(g_epids)/sizeof(g_epids[0]);
  /*return 1;*/
}

static psm_epid_t* get_epids(){
  return g_epids;
}


static inline psm_epaddr_t __get_ep(psm_net_ch_t *ch, int rank){
  assert(rank < 2);
  return ch->eps[rank];
}

static psm_epaddr_t get_peer_ep(psm_net_ch_t *ch){
  return __get_ep(ch, ch->rank_peer);
}

static psm_epaddr_t get_local_ep(psm_net_ch_t *ch){
  return __get_ep(ch, ch->rank_self);
}

static uint64_t get_ch_tag(psm_net_ch_t *ch){
  return 0xbaafeed;
}

static bool is_rdma_active(){
  bool isactive = false;
  if (!strcmp(host, "dagger01")) {
    isactive = true;
  }
  return isactive;
}
// we mask our local endpoint
// so that ep_connect won't attempt local loopback
static const int* get_epmask(){
  if (!strcmp(host, "dagger01")) {
    g_epmask[0] = 0;
  } else {
    g_epmask[1] = 0;
  }
  return (const int*) g_epmask;
}

static int get_my_rank(){
  int r;
  if (!strcmp(host, "dagger01")) {
    r = 0;
  } else {
    r = 1;
  }
  return r;
}

int connect_eps(psm_net_ch_t *ch) {

  int i, ret, fret = PSM_OK;
  int num_ep = get_num_epids_known();
  psm_error_t *errors = (psm_error_t *) calloc(num_ep, sizeof(psm_error_t));
  psm_epid_t *ids = get_epids();
  const int* mask =  get_epmask();

  // debug
#if 0
  printf("hostname=%s num_ep=%d ep_id[mine]=%lx\n", host, num_ep, ids[get_my_rank()]);
#endif
  for (i = 0; i < num_ep; ++i) {
    ret = psm_ep_connect(ch->ep, num_ep, ids,
		   mask,  // We want to connect all epids, no mask needed
		   errors, ch->eps, 30 * 1e9);
    if(ret != PSM_OK){
      fret = ret;
    }
  }

  return fret;
}

static uint64_t get_channel_sz(){
  return ch_size;
}

static int get_channel_unit(){
  return ch_unit;
}

static void* get_base(bool remote){
  if(!local_base){
    local_base = malloc(get_channel_sz());
  }
  return remote? remote_base: local_base;
}

static int __init_alloc(ralloc_t *alloc, void *base, uint64_t size, int unit){
  // simple next-fit allocator
  alloc->base = base;
  alloc->size = size;
  alloc->blks = (size/unit);
  alloc->unit = unit;
  alloc->next = 0;
  alloc->free = alloc->blks;
  return 0;
}

typedef struct rdesc {
  void *lbuf;
  void *rbuf;
  uint32_t len;
} rdesc_t;

static void* getmem(rdesc_t d){
  return d.lbuf;
}

rdesc_t rmalloc(psm_net_ch_t *ch, uint32_t bytes){
  rdesc_t ret = {.lbuf = NULL, .rbuf = NULL};
  if(ch->l_allocator->next >= ch->l_allocator->free){
    fprintf(stderr, "cannot allocate memory right now!\n");
    goto rmalloc_ret;
  }
  uint32_t blks_needed = bytes/ch->l_allocator->unit;
  uint8_t *next_buf = ch->l_allocator->base + (ch->l_allocator->next * ch->l_allocator->unit);
  ch->l_allocator->next += blks_needed;
  ret.lbuf = next_buf;
  ret.len  = bytes;
rmalloc_ret:
  return ret;
}

int progress_reqs(psm_mq_t mq, psm_mq_req_t *req) {
  int num_completed = 0;
  /*psm_mq_req_t    req;*/
  psm_mq_status_t status;
  psm_error_t     err;
  /*my_request_t *  myreq;*/

  do {
    err = psm_mq_ipeek(mq, req, NULL);
    if (err == PSM_MQ_NO_COMPLETIONS)
      return num_completed;
    else if (err != PSM_OK)
      goto progress_reqs_err;
    num_completed++;

    // We reached a point where req is guaranteed to complete
    // We obtained 'req' at the head of the completion queue.  We can
    // now free the request with PSM and obtain our original reques
    // from the status' context
    err = psm_mq_test(req, &status);
    /*myreq = (my_request_t *)status.context;*/

    // handle the completion for myreq whether myreq is a posted receive
    // or a non-blocking send.
  } while (1);
progress_reqs_err:
  printf("Error in request progress loop\n");
  return -1;
}

psm_error_t psm_send_rdma_write(psm_net_ch_t *ch, void* lbuf, uint32_t len, uint64_t tag){
  psm_error_t ret = psm_mq_send(ch->mq, get_peer_ep(ch), 0, tag, lbuf, len);
  return ret;
}

#define TAG_SEL_ALL ((1ULL << 31) - 1)
psm_error_t psm_recv_rdma_read(psm_net_ch_t *ch, void* lbuf, uint32_t len, uint64_t tag, psm_mq_req_t *req){
  psm_error_t ret = psm_mq_irecv(ch->mq, tag, TAG_SEL_ALL, 0, lbuf, len, NULL, req);
  return ret;
}

int rwrite(psm_net_ch_t *ch, rdesc_t ret){
  /*rdesc_t ret = rmalloc(ch, bytes);*/
  if(ret.lbuf){
    psm_error_t code = psm_send_rdma_write(ch, ret.lbuf, ret.len, get_ch_tag(ch) );
    if(code != PSM_OK){
      fprintf(stderr, "rwrite() send failed\n");
      return -1;
    }
  } else {
    fprintf(stderr, "rwrite() allocation failed:full\n");
  }
  return 0;
}

void* rread(psm_net_ch_t *ch, uint32_t bytes){
  rdesc_t ret = rmalloc(ch, bytes);
  int comp = 0;
  if(ret.lbuf){
    psm_mq_req_t req;
    psm_error_t code = psm_recv_rdma_read(ch, ret.lbuf, bytes, get_ch_tag(ch), &req);
    if(code != PSM_OK){
      fprintf(stderr, "rread() recv failed\n");
      return NULL;
    }
    while(!comp){
      comp = progress_reqs(ch->mq, &req);
      if(comp == -1){
        return NULL;
      }
    }
    
  } else {
    fprintf(stderr, "rread() allocation failed:full\n");
  }
  return ret.lbuf;
}

void init_channel_allocators(psm_net_ch_t *ch){
  if(is_rdma_active()){
    ch->r_allocator = calloc(1, sizeof(ralloc_t));
    __init_alloc(ch->r_allocator, get_base(true), get_channel_sz(), get_channel_unit());
  } else {
    ch->r_allocator = NULL;
  }
  // local
  ch->l_allocator = calloc(1, sizeof(ralloc_t));
  __init_alloc(ch->l_allocator, get_base(false), get_channel_sz(), get_channel_unit());
}

int init_channel(psm_net_ch_t *ch) {
  psm_uuid_t job = "deadbeef";
  ch->eps = calloc(MAX_EP_ADDR, sizeof(psm_epaddr_t));
  ch->rank_self = get_my_rank();
  ch->rank_peer = (get_my_rank()+1)%2;

  init_channel_allocators(ch);
  //psm_uuid_generate(job);
  int ret = try_to_initialize_psm(ch, job);
  // sleeping because we dont have a barrier here
  sleep(2);
  if(ret == PSM_OK){
    ret  = connect_eps(ch);
  }
  return ret;
}

int run_test(psm_net_ch_t *ch){
  int  size = sizeof(int);
  int *tmp;
  int  i, N = 2;

  if (is_rdma_active()) {
    for (i = 0; i < N; ++i) {
      rdesc_t ret = rmalloc(ch, size);
      tmp	 = getmem(ret);
      *tmp	= 1024 + i;
      printf("rwrite base [%p] alloc [%p] val=%d\n", ch->l_allocator->base, tmp,
	     *tmp);
      rwrite(ch, ret);
    }
  } else {
    for (i = 0; i < N; ++i) {
      tmp = rread(ch, size);
      printf("rread base [%p] alloc [%p] val=%d\n", ch->l_allocator->base, tmp,
	     *tmp);
    }
  }
  return 0;
}

int main() {
  psm_net_ch_t ch;
  bool	 isactive = false;
  gethostname(host, 512);

  if (is_rdma_active()) {
    isactive = true;
  }

  int ret = init_channel(&ch);

  if (is_rdma_active()) {
    isactive = true;
  }

  run_test(&ch);
  printf("[%s] rank=%d peer=%d active?%d init PSM=%d PSM_VER=%u [%x] PSM_EPID %llu [%llx]\n",
	 host, ch.rank_self, ch.rank_peer, isactive, ret, PSM_VERNO, PSM_VERNO, ch.epid, ch.epid);
  /*psm_finalize();*/
  return 0;
}

