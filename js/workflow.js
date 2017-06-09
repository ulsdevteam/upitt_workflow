/**
 * @file
 * Javascript file for pitt_workflow
 */

jQuery( document ).ready(function() {
    var batch_host = jQuery('#edit-batch-host').val();
    if (batch_host == 'ftp') {
      var protocol = window.location.protocol;
      var hostname = window.location.host;
      host_selected('ftp', protocol + '://' + hostname);
    }
});

function host_selected(control, site_url) {
  var ftp_selected = (control == 'ftp' || control.value == 'ftp');
  var initial_path = jQuery('#edit-initial-batch-host').val();
  jQuery('#edit-box-selector option').remove();

  jQuery('#edit-loading').val('...');

  if (ftp_selected) {
    jQuery('#edit-local-files').hide();
    jQuery('#edit-box-files').show();
  }
  else {
    jQuery('#edit-box-files').hide();
    jQuery('#edit-local-files').show();
  } 
  var current_path = jQuery('[name="initial_ftp_path"]').val();  
  var get_ftp_files_url = site_url + '/ajax/workflow/browse_ftp/' + current_path;

  jQuery('#edit-loading').val('done!');
  
  var ajaxcall = jQuery.get(get_ftp_files_url, function( data ) {
    jQuery('#edit-box-files').html( data );
  });
          
}

function ftp_change_path(path, site_url) {
  var get_ftp_files_url = site_url + '/ajax/workflow/browse_ftp/' + path;
  var ajaxcall = jQuery.get(get_ftp_files_url, function( data ) {
    jQuery('#edit-box-files').html( data );
    jQuery('[name="initial_ftp_path"]').val('ftp:' + path);
  });
}

