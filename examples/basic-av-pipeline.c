#include <gst/gst.h>

typedef struct _CustomData
{
  GstElement *pipeline;
  GstElement *source;

  GstElement *audio_queue, *video_queue;
  GstElement *audio_tee, *video_tee;

  GstElement *audio_queue_record, *video_queue_record;
  GstElement *audio_queue_display, *video_queue_display;

  GstElement *audio_encoder, *video_encoder;
  GstElement *muxer;
  GstElement *file_sink;

  GstElement *audio_convert, *audio_sink;
  GstElement *video_convert, *video_sink;
} CustomData;

static void pad_added_handler (GstElement * src, GstPad * pad,
    CustomData * data);

int
main (int argc, char *arvg[])
{
  CustomData data;
  GstBus *bus;
  GstMessage *msg;
  GstStateChangeReturn ret;
  gboolean terminate = FALSE;

  gst_init (&argc, &arvg);

  data.source = gst_element_factory_make ("uridecodebin", "source");

  data.audio_queue = gst_element_factory_make ("queue", "audio-queue");
  data.video_queue = gst_element_factory_make ("queue", "video-queue");

  data.audio_tee = gst_element_factory_make ("tee", "audio-tee");
  data.video_tee = gst_element_factory_make ("tee", "video-tee");

  data.audio_queue_record =
      gst_element_factory_make ("queue", "audio-queue-record");
  data.video_queue_record =
      gst_element_factory_make ("queue", "video-queue-record");

  data.audio_queue_display =
      gst_element_factory_make ("queue", "audio-queue-display");
  data.video_queue_display =
      gst_element_factory_make ("queue", "video-queue-display");

  data.audio_encoder = gst_element_factory_make ("voaacenc", "audio-encoder");
  data.video_encoder = gst_element_factory_make ("x264enc", "video-encoder");

  data.muxer = gst_element_factory_make ("mp4mux", "muxer");
  data.file_sink = gst_element_factory_make ("filesink", "file-sink");

  data.audio_convert =
      gst_element_factory_make ("audioconvert", "audio-convert");
  data.audio_sink = gst_element_factory_make ("autoaudiosink", "audio-sink");

  data.video_convert =
      gst_element_factory_make ("videoconvert", "video-convert");
  data.video_sink = gst_element_factory_make ("autovideosink", "video-sink");

  data.pipeline = gst_pipeline_new ("stream-pipeline");

  if (!data.pipeline || !data.source || !data.audio_queue || !data.video_queue
      || !data.audio_tee || !data.video_tee || !data.audio_queue_record
      || !data.video_queue_record || !data.audio_queue_display
      || !data.video_queue_display || !data.audio_encoder || !data.video_encoder
      || !data.muxer || !data.file_sink || !data.audio_convert
      || !data.audio_sink || !data.video_convert || !data.video_sink) {
    g_printerr ("Not all elements could be created.\n");
    return -1;
  }

  g_object_set (data.source, "uri",
      "https://gstreamer.freedesktop.org/data/media/sintel_trailer-480p.webm",
      NULL);
  g_object_set (data.file_sink, "location", "./rec.mp4", NULL);

  gst_bin_add_many (GST_BIN (data.pipeline),
      data.source,
      data.audio_queue, data.video_queue,
      data.audio_tee, data.video_tee,
      data.audio_queue_record, data.video_queue_record,
      data.audio_queue_display, data.video_queue_display,
      data.audio_encoder, data.video_encoder,
      data.muxer, data.file_sink,
      data.audio_convert, data.audio_sink,
      data.video_convert, data.video_sink, NULL);

  if (!gst_element_link_many (data.audio_queue, data.audio_tee, NULL) ||
      !gst_element_link_many (data.video_queue, data.video_tee, NULL)) {
    g_printerr ("Failed to link queues to tee.\n");
    return -1;
  }

  if (!gst_element_link_many (data.audio_queue_display, data.audio_convert,
          data.audio_sink, NULL)
      || !gst_element_link_many (data.video_queue_display, data.video_convert,
          data.video_sink, NULL)) {
    g_printerr ("Failed to link tee display branches.\n");
    return -1;
  }

  if (!gst_element_link_many (data.audio_queue_record, data.audio_encoder, NULL)
      || !gst_element_link_many (data.video_queue_record, data.video_encoder,
          NULL)) {
    g_printerr ("Failed to link tee record branches.\n");
    return -1;
  }

  if (!gst_element_link_many (data.muxer, data.file_sink, NULL)) {
    g_printerr ("Failed to link muxer to file sink.\n");
    return -1;
  }

  GstPad *tee_audio_src_pad1 =
      gst_element_request_pad_simple (data.audio_tee, "src_%u");
  GstPad *tee_audio_src_pad2 =
      gst_element_request_pad_simple (data.audio_tee, "src_%u");

  GstPad *display_audio_sink_pad =
      gst_element_get_static_pad (data.audio_queue_display, "sink");
  GstPad *record_audio_sink_pad =
      gst_element_get_static_pad (data.audio_queue_record, "sink");

  gst_pad_link (tee_audio_src_pad1, display_audio_sink_pad);
  gst_pad_link (tee_audio_src_pad2, record_audio_sink_pad);

  GstPad *tee_video_src_pad1 =
      gst_element_request_pad_simple (data.video_tee, "src_%u");
  GstPad *tee_video_src_pad2 =
      gst_element_request_pad_simple (data.video_tee, "src_%u");

  GstPad *display_video_sink_pad =
      gst_element_get_static_pad (data.video_queue_display, "sink");
  GstPad *record_video_sink_pad =
      gst_element_get_static_pad (data.video_queue_record, "sink");

  gst_pad_link (tee_video_src_pad1, display_video_sink_pad);
  gst_pad_link (tee_video_src_pad2, record_video_sink_pad);


  g_signal_connect (data.source, "pad-added", G_CALLBACK (pad_added_handler),
      &data);

  ret = gst_element_set_state (data.pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr ("Unable to set the pipeline to the playing state. \n");
    gst_object_unref (data.pipeline);
    return -1;
  }

  bus = gst_element_get_bus (data.pipeline);
  do {
    msg = gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE,
        GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

    if (msg != NULL) {
      GError *err;
      gchar *debug_info;

      switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_ERROR:
          gst_message_parse_error (msg, &err, &debug_info);
          g_printerr ("Error received from element %s: %s\n",
              GST_OBJECT_NAME (msg->src), err->message);
          g_printerr ("Debugging information: %s\n",
              debug_info ? debug_info : "none");
          g_clear_error (&err);
          g_free (debug_info);
          terminate = TRUE;
          break;
        case GST_MESSAGE_EOS:
          g_print ("End-Of-Stream reached.\n");
          terminate = TRUE;
          break;
        case GST_MESSAGE_STATE_CHANGED:
          if (GST_MESSAGE_SRC (msg) == GST_OBJECT (data.pipeline)) {
            GstState old_state, new_state, pending_state;
            gst_message_parse_state_changed (msg, &old_state, &new_state,
                &pending_state);
            g_print ("Pipeline state changed from %s to %s:\n",
                gst_element_state_get_name (old_state),
                gst_element_state_get_name (new_state));
          }
          break;
        default:
          g_printerr ("Unexpected message received.\n");
          break;
      }
      gst_message_unref (msg);
    }
  } while (!terminate);

  gst_object_unref (bus);
  gst_element_set_state (data.pipeline, GST_STATE_NULL);
  gst_object_unref (data.pipeline);
  return 0;
}

static void
pad_added_handler (GstElement *src, GstPad *new_pad, CustomData *data)
{
  GstPad *sink_pad;
  GstPadLinkReturn ret;
  GstCaps *new_pad_caps = NULL;
  GstStructure *new_pad_struct = NULL;
  const gchar *new_pad_type = NULL;

  GstPad *enc_src, *mux_sink;

  g_print ("Received new pad '%s' from '%s':\n", GST_PAD_NAME (new_pad),
      GST_ELEMENT_NAME (src));

  if (gst_pad_is_linked (sink_pad)) {
    g_print ("We are already linked. Ignoring.\n");
    goto exit;
  }

  new_pad_caps = gst_pad_get_current_caps (new_pad);
  new_pad_struct = gst_caps_get_structure (new_pad_caps, 0);
  new_pad_type = gst_structure_get_name (new_pad_struct);

  if (g_str_has_prefix (new_pad_type, "audio/x-raw")) {
    sink_pad = gst_element_get_static_pad (data->audio_queue, "sink");
    g_print ("It has type '%s' which is raw audio\n", new_pad_type);
  } else if (g_str_has_prefix (new_pad_type, "video/x-raw")) {
    sink_pad = gst_element_get_static_pad (data->video_queue, "sink");
    g_print ("It has type '%s' which is raw video\n", new_pad_type);
  } else {
    g_print
        ("It has type '%s' which is not raw audio and raw video. Ignoring.\n",
        new_pad_type);
    goto exit;
  }

  ret = gst_pad_link (new_pad, sink_pad);
  if (GST_PAD_LINK_FAILED (ret)) {
    g_print ("Type is '%s' but link failed.\n", new_pad_type);
  } else {
    g_print ("Link succeeded (type '%s').\n", new_pad_type);
  }

  if (g_str_has_prefix (new_pad_type, "audio/x-raw")) {
    enc_src = gst_element_get_static_pad (data->audio_encoder, "src");
    mux_sink = gst_element_request_pad_simple (data->muxer, "audio_%u");
    if (gst_pad_link (enc_src, mux_sink) != GST_PAD_LINK_OK) {
      g_printerr ("Failed to link audio encoder to muxer\n");
    }
  } else if (g_str_has_prefix (new_pad_type, "video/x-raw")) {
    enc_src = gst_element_get_static_pad (data->video_encoder, "src");
    mux_sink = gst_element_request_pad_simple (data->muxer, "video_%u");
    if (gst_pad_link (enc_src, mux_sink) != GST_PAD_LINK_OK) {
      g_printerr ("Failed to link video encoder to muxer\n");
    }
  }

exit:
  if (new_pad_caps != NULL)
    gst_caps_unref (new_pad_caps);

  gst_object_unref (sink_pad);
  gst_object_unref (enc_src);
  gst_object_unref (mux_sink);
}
