// Copyright (C) 2020  Aleksandr kolodki <alexandr.kolodkin@gmail.com>
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.


function show_alert(message, msg_type = "danger") {

	// Delete all previous alert
	$(".alert").remove();

	// Create new alert
	$("#save").parent().after(`
		<div class="alert alert-${msg_type} alert-dismissible fade show" role="alert">
  			<strong>${message}</strong>
  			<button type="button" class="close" data-dismiss="alert" aria-label="Close">
    			<span aria-hidden="true">&times;</span>
  			</button>
		</div>
	`);
}

function scan_networks() {
	$.ajax({
		url: "api",
		data: {
			command: "scan"
		},
		dataType: "json",
		timeout: 10000,
		success: function (output) {
			if (typeof(output.wifi) !== 'undefined') {
				// Save selected option
				var ssid = $("#ssid option:selected").val();

				// Remove existing options
				$(".ssid_option").remove();

				// Add found netwoks
				for (ssid_info in output.wifi) {
					var html;
					if (output.wifi[ssid_info].ssid === ssid) {
						html = `<option class="ssid_option" selected>${output.wifi[ssid_info].ssid}</option>`;
					} else {
						html = `<option class="ssid_option">${output.wifi[ssid_info].ssid}</option>`;
					}
					$("#ssid").append(html);
				}

			} else {
				setTimeout(scan_networks, 10000);
			}
		},
		error: function(xmlhttprequest, textstatus, message) {
			setTimeout(scan_networks, 10000);
		}
	});
}

$(document).ready(function () {

	scan_networks();

	$("#home-tab").on("click", function (e) {
		$(".nav-link").removeClass("active");
		$(".tab-pane").hide();
		$("#home").show();
		$("#navbar").collapse('hide');
	});

	$("#wifi-tab").on("click", function (e) {
		$(".nav-link").removeClass("active");
		$(".tab-pane").hide();
		$("#wifi").show();
		$("#navbar").collapse('hide');
	});

	$("#stepper-tab").on("click", function (e) {
		$(".nav-link").removeClass("active");
		$(".tab-pane").hide();
		$("#stepper").show();
		$("#navbar").collapse('hide');
	});

	$("#status-tab").on("click", function (e) {
		$(".nav-link").removeClass("active");
		$(".tab-pane").hide();
		$("#status").show();
		$("#navbar").collapse('hide');
	});

	$("#move90cw").on("click", function (e) {
	   e.preventDefault();
	   $.ajax({
		   url: "api",
		   data: {
			   command: "stepper",
			   mode: "movecw",
			   value: 90
		   }
	   });
	});

	
	$("#move90ccw").on("click", function (e) {
		e.preventDefault();
		$.ajax({
			url: "api",
			data: {
				command: "stepper",
				mode: "moveccw",
				value: 90
			}
		});
	});

	 
	$("#move180cw").on("click", function (e) {
		e.preventDefault();
		$.ajax({
			url: "api",
			data: {
				command: "stepper",
				mode: "movecw",
				value: 180
			}
		});
	});

	 
	$("#move180ccw").on("click", function (e) {
		e.preventDefault();
		$.ajax({
			url: "api",
			data: {
				command: "stepper",
				mode: "moveccw",
				value: 180
			}
		});
	});

	 
	$("#move360cw").on("click", function (e) {
		e.preventDefault();
		$.ajax({
			url: "api",
			data: {
				command: "stepper",
				mode: "movecw",
				value: 360
			}
		});
	});

	 
	$("#move360ccw").on("click", function (e) {
		e.preventDefault();
		$.ajax({
			url: "api",
			data: {
				command: "stepper",
				mode: "moveccw",
				value: 360
			}
		});
	});

	// Request networs
	$("#scan").on("click", function (e) {
		e.preventDefault();
		scan_networks();
	});

	// Request reboot
	$("#reboot").on("click", function (e) {
		e.preventDefault();
		$.ajax({
			url: "api",
			data: {
				command: "reboot"
			},
			dataType: "json",
			timeout: 10000,
			success: function (output) {
				// Show alert if requst complited
				if (typeof(output.result) === 'undefined') {
					show_alert("Settings isn't saved. Unknown error.");
				} else {
					if (output.result == 'Ok') {
						show_alert("Settings saved successfully.", "success");
					}
				}
			},
			error: function(xmlhttprequest, textstatus, message) {
				if (textstatus === "error") {
					show_alert("Settings isn't saved. Server error: " + message + ".");
				} else if (textstatus === "timeout") {
					show_alert("Settings isn't saved. Server not respond.");
				}
			}
		});
	});

	// SSID selection changed
	$("#ssid").change(function (){
		if ($("#ssid option:selected").val() == $("#manual-ssid-option").val()) {
			$("#manual-ssid").show().focus();
		} else {
			$("#manual-ssid").hide();
		}
	});
	
	// Save new network SSID & password
	$("#save").on("click", function (e) {
		e.preventDefault();

		// SSID
		var ssid = $("#ssid option:selected").text();
		if (ssid == $("#manual-ssid-option").text()) ssid = $("#manual-ssid").val();

		// Password
		var pass = $("#wifi-password").val();

		if (ssid == "") {
			// Show alert if SSID is empty
			show_alert("SSID must be filled out!");
		} else if (pass == "") {
			// Show alert if password is empty
			show_alert("Password must be filled out!");
		} else {

			// Send request
			$.ajax({
				url: "api",
				data: {
					command: "save",
					ssid: ssid,
					pass: pass,
				},
				dataType: "json",
				timeout: 10000,
				success: function (output) {
					// Show alert if requst complited
					if (typeof(output.result) === 'undefined') {
						show_alert("Settings isn't saved. Unknown error.");
					} else {
						if (output.result == 'Ok') {
							show_alert("Settings saved successfully.", "success");
						} else {
							show_alert("Settings isn't saved. " + output.error);
						}
					}
				},
				error: function(xmlhttprequest, textstatus, message) {
					if (textstatus === "error") {
						show_alert("Settings isn't saved. Server error: " + message + ".");
					} else if (textstatus === "timeout") {
						show_alert("Settings isn't saved. Server not respond.");
					}
				}
			});
		}
	});
});