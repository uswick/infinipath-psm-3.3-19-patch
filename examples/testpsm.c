#include <stdio.h>
#include <stdlib.h>
#include <psm.h>

typedef struct psm_net_ch {
  psm_ep_t ep;
  psm_epid_t epid;
} psm_net_ch_t;

int try_to_initialize_psm(psm_net_ch_t *ch, psm_uuid_t job_uuid) {
      int verno_major = PSM_VERNO_MAJOR;
      int verno_minor = PSM_VERNO_MINOR;
 
      int err = psm_error_register_handler(NULL,  // Global handler
                                   PSM_ERRHANDLER_NO_HANDLER); // return errors
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
             fprintf(stderr, "PSM loaded an unexpected/unsupported "
                             "version (%d.%d)\n", verno_major, verno_minor);
         return -1;
      }
 
      // We were able to initialize PSM but will defer all further error
      // handling since most of the errors beyond this point will be fatal.
      err = psm_error_register_handler(NULL,  // Global handler
                                   PSM_ERRHANDLER_PSM_HANDLER); // 
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
     epopts.timeout = 151e9;  // 15 second timeout
     epopts.unit = -1;	// We want a specific unit, -1 would let PSM
                               // choose the unit for us.
     epopts.port = 0;	// We want a specific unit, <= 0 would let PSM
     epopts.network_pkey = PSM_EP_OPEN_PKEY_DEFAULT;
                               // choose the port for us.
     // We've already set affinity, don't let PSM do so if it wants to.
     if (epopts.affinity == PSM_EP_OPEN_AFFINITY_SET)
        epopts.affinity = PSM_EP_OPEN_AFFINITY_SKIP;
 

      //job_uuid[0] = 0x1;
      //if((err = psm_ep_open(job_uuid, NULL, &ep, &epid)) != PSM_OK) {
      if((err = psm_ep_open(job_uuid, &epopts, &ch->ep, &ch->epid)) != PSM_OK) {
          fprintf(stderr, "psm_ep_open failed with error %s\n", psm_error_get_string(err));
          //MPIU_ERR_SETANDJUMP(mpi_errno, MPI_ERR_OTHER, "**psmepopen");
	  return -1;
      }
      return 1;
}



int main(){
  psm_net_ch_t ch;
  psm_uuid_t job; 
  psm_uuid_generate(job);
  int ret = try_to_initialize_psm(&ch, job); 
  printf("init PSM=%d PSM_VER=%u %x\n", ret, PSM_VERNO, PSM_VERNO);
  return 0;
}

