#include <psm.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char	 host[512];

typedef struct psm_net_ch {
  psm_ep_t      ep;
  psm_epid_t    epid;
  psm_epaddr_t *eps;  // array of endpoint addresses indexed from 0..n
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
  printf("hostname=%s num_ep=%d ep_id[mine]=%lx\n", host, num_ep, ids[get_my_rank()]);
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

int init_channel(psm_net_ch_t *ch) {
  psm_uuid_t job = "deadbeef";
  ch->eps = calloc(MAX_EP_ADDR, sizeof(psm_epaddr_t));

  //psm_uuid_generate(job);
  int ret = try_to_initialize_psm(ch, job);
  // sleeping because we dont have a barrier here
  sleep(1);
  if(ret == PSM_OK){
    ret  = connect_eps(ch);
  }
  return ret;
}

int main() {
  psm_net_ch_t ch;
  bool	 isactive = false;
  gethostname(host, 512);

  if (!strcmp(host, "dagger01")) {
    isactive = true;
  }

  int ret = init_channel(&ch);
  printf("[%s] active?%d init PSM=%d PSM_VER=%u [%x] PSM_EPID %llu [%llx]\n",
	 host, isactive, ret, PSM_VERNO, PSM_VERNO, ch.epid, ch.epid);
  /*psm_finalize();*/
  return 0;
}

