/**
 * @file
 * Javascript file for islandora solr extras
 */

function host_selected(control, site_url) {
  var ftp_selected;
  // 
  var initial_path;
  initial_path = jQuery('#edit-initial-batch-host').val();
  jQuery('#edit-box-selector option').remove();
  jQuery('#edit-box-selector').append('<option value="foo" selected="selected">Foo</option>');
  jQuery('#edit-box-selector').append('<option value="bar" selected="selected">BAaaar</option>');

  ftp_selected = (control.value == 'ftp');
  if (ftp_selected) {
    jQuery('#edit-local-files').hide();
    jQuery('#edit-box-files').show();
  }
  else {
    jQuery('#edit-box-files').hide();
    jQuery('#edit-local-files').show();
  } 
  /* var test_string = jQuery.ajax({
  url: 'http://infost01-02.library.pitt.edu:8000/workflow/batch/edit/1429',
  context: document.body,
  });*/
  var current_path = jQuery('[name="initial_batch_host"]').val();
  current_path = current_path.replace("/", "|");
  
  var get_ftp_files_url = site_url + '/ajax/workflow/browse_ftp/' + current_path;
  console.log(get_ftp_files_url);
  
  var ajaxcall = jQuery.get(get_ftp_files_url, function( data ) {
    jQuery('#edit-box-files').html( data );
  });
          
}
