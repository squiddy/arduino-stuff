#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>

#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "mp_msg.h"
#include "vf.h"

#define SERVER_HOST "localhost"
#define SERVER_PORT 5005
#define CHANNEL_COUNT 1

typedef struct {
    unsigned char channel_count;
    unsigned char y[CHANNEL_COUNT];
    unsigned char v[CHANNEL_COUNT];
    unsigned char u[CHANNEL_COUNT];
} valo_packet_t;

typedef struct vf_priv_s {
    valo_packet_t packet;
    int fd;
} vf_priv_t;

typedef struct {
    int avg_y;
    int avg_v;
    int avg_u;
} channel_data;

static int packet_size = sizeof(valo_packet_t);
static channel_data channels[CHANNEL_COUNT];

//===========================================================================//

static int udp_init(struct vf_priv_s *priv) {
	struct sockaddr_in addr;
	struct hostent *dest;

	dest = gethostbyname(SERVER_HOST);

	priv->fd = -1;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(SERVER_PORT);

	memcpy(&addr.sin_addr.s_addr, dest->h_addr_list[0], dest->h_length);

	priv->fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	connect(priv->fd, (struct sockaddr *)&addr, sizeof(addr));

	return 0;
}

static void udp_send(struct vf_priv_s *priv)
{
    if (send(priv->fd, &priv->packet, packet_size, 0) != packet_size) {
		mp_msg(MSGT_VFILTER, MSGL_ERR, "unable to send\n");
    }
}

static void udp_close(struct vf_priv_s *priv)
{
    close(priv->fd);
}

//===========================================================================//

static inline void average(channel_data *channel, int size) {
    channel->avg_y = (int)(channel->avg_y / size);
    channel->avg_v = (int)(channel->avg_v / size);
    channel->avg_u = (int)(channel->avg_u / size);
}

static int put_image(struct vf_instance *vf, mp_image_t *mpi, double pts)
{
    mp_image_t *dmpi;
    int y, x, i;
    int image_size = 0.52f * mpi->w * mpi->h;

    dmpi = vf_get_image(vf->next, mpi->imgfmt, MP_IMGTYPE_EXPORT, 0, mpi->w, mpi->h);
    dmpi->planes[0] = mpi->planes[0];
    dmpi->planes[1] = mpi->planes[1];
    dmpi->planes[2] = mpi->planes[2];
    dmpi->stride[0] = mpi->stride[0];
    dmpi->stride[1] = mpi->stride[1];
    dmpi->stride[2] = mpi->stride[2];
    dmpi->width = mpi->width;

    memset(&channels, 0, sizeof(channels));

    for(y = 0; y < mpi->h; y++) {
        for(x = 0; x < mpi->w; x++) {
            if ((y >= 0.2f * mpi->h) && (x >= 0.2f * mpi->w && x <= 0.8f * mpi->w)) {
                continue;
            }

            unsigned char* s = mpi->planes[0] + mpi->stride[0] * y;
            channels[0].avg_y += s[x];

            s = mpi->planes[1] + mpi->stride[1] * (y >> 1);
            channels[0].avg_v += s[x >> 1];

            s = mpi->planes[2] + mpi->stride[2] * (y >> 1);
            channels[0].avg_u += s[x >> 1];
        }
    }

    average(&channels[0], image_size);

    for(i = 0; i < CHANNEL_COUNT; i++) {
        vf->priv->packet.y[i] = channels[i].avg_y;
        vf->priv->packet.v[i] = channels[i].avg_v;
        vf->priv->packet.u[i] = channels[i].avg_u;
    }

    udp_send(vf->priv);

    vf_clone_mpi_attributes(dmpi, mpi);

    return vf_next_put_image(vf, dmpi, pts);
}

//===========================================================================//

static void uninit(vf_instance_t *vf)
{
    udp_close(vf->priv);

    free(vf->priv);
}

static int vf_open(vf_instance_t *vf, char *args)
{
    vf->priv = malloc(sizeof(struct vf_priv_s));
    vf->priv->packet.channel_count = CHANNEL_COUNT;

    udp_init(vf->priv);

    vf->put_image = put_image;
    vf->uninit = uninit;

    return 1;
}

const vf_info_t vf_info_valo = {
    "valo",
    "DIY ambilight",
    "squiddy",
    "",
    vf_open,
    NULL
};
