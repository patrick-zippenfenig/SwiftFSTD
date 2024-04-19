import XCTest
@testable import SwiftFSTD
import CFSTD

final class SwiftFSTDTests: XCTestCase {
    func testExample() throws {
        // XCTest Documentation
        // https://developer.apple.com/documentation/xctest

        // Defining Test Cases and Test Methods
        // https://developer.apple.com/documentation/xctest/defining_test_cases_and_test_methods
        
        // Set library verbosity level
          // c_fstopc("MSGLVL", "ERRORS", 0);

           // Get all values as 32bits at most.  If you want full field
           // precision, you need to consider the data type and allocate the
           // corresponding amount of memory
           //c_fstopl("REDUCTION32", 1, 0);

        let filePath = "/Users/patrick/Downloads/2023060100_002"
        
        // Link file to a unit number (Fortran compatible I/O handle)
    var unitNb: Int32 = 0;
        var resultCode = c_fnom(&unitNb, filePath, "STD+OLD+R/O", 0);
        if (resultCode < 0) {
            print("Failed to link file \(filePath)");
            return
        }

        // Open the file
        var rnd = "RND".utf8CString
        resultCode = rnd.withUnsafeMutableBytes({
            c_fstouv(unitNb, $0.assumingMemoryBound(to: CChar.self).baseAddress!)
        })
        if (resultCode < 0) {
            print("Failed to open the file \(filePath)");
            return;
        }

        let recordNb = c_fstnbr(unitNb);
        print("There are \(recordNb) records in \(filePath)");
        
        
        // Dimension along X
        var ni: UInt32 = 0
            // Dimension along Y
        var nj: UInt32 = 0
            // Dimension along Z
        var nk: UInt32 = 0
            // Forecast origin date
        var dateo: UInt32 = 0
            // Forecast timestep in seconds
        var deet: UInt32 = 0
            // Number of forecast steps.
        var npas: UInt32 = 0
            // Number of bits per field element
        var nbits: UInt32 = 0
            // Data type
        var datyp: UInt32 = 0
            // Vertical level
        var ip1: UInt32 = 0
            // Second field identifier.  Usually the forecast hour
        var ip2: UInt32 = 0
            // Third field identifier.  User defined
        var ip3: UInt32 = 0
        
            // Variable type
        var typvar = [CChar](repeating: 32, count: 3) + [0]
            // Variable name
        var nomvar = [CChar](repeating: 32, count: 5) + [0]
            // Label
        var etiket = [CChar](repeating: 32, count: 13) + [0]
            // Grid type (geographical projection)
        var grtyp = [CChar](repeating: 32, count: 2) + [0]
            // First grid descriptor
        var ig1: UInt32 = 0
            // Second grid descriptor
        var ig2: UInt32 = 0
            // Third grid descriptor
        var ig3: UInt32 = 0
            // Fourth grid descriptor
        var ig4: UInt32 = 0
            // ???
        var swa: UInt32 = 0
            // Header length (in 64 bits units)
        var lng: UInt32 = 0
            // ???
        var dltf: UInt32 = 0
            // ???
        var ubc: UInt32 = 0
            // ???
        var extra1: UInt32 = 0, extra2: UInt32 = 0, extra3: UInt32 = 0

        var lat: Float = 0;
        var lon: Float = 0;

            print("nomvar\ttypvar\tgrtyp\tni\tnj\tnk\tip1\tip2\tip3\tetiket\tig1\tig2\tig3\tig4\n");

            // This function searches for field in the file.  An empty string for
            // string params and -1 for integers means everything
            var key = c_fstinf(unitNb, &ni, &nj, &nk, -1, "", -1, -1, -1, "", "");
        repeat {
                print("\tkey=%d\n", key);

                c_fstprm (key, &dateo, &deet, &npas, &ni, &nj, &nk, &nbits, &datyp, &ip1, &ip2, &ip3, &typvar, &nomvar, &etiket, &grtyp, &ig1, &ig2, &ig3, &ig4, &swa, &lng, &dltf, &ubc, &extra1, &extra2, &extra3);

            print("%s\t%s\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%s\t%d\t%d\t%d\t%d\n", String(cString: nomvar), String(cString:typvar), String(cString:grtyp), ni, nj, nk, ip1, ip2, ip3, String(cString:etiket), ig1, ig2, ig3, ig4);

                // Allocate memory to hold the field's data
            var field = [Float](repeating: .nan, count: Int(ni * nj * nk))

                resultCode = c_fstluk(&field, key, &ni, &nj, &nk);
                if (resultCode < 0) {
                    print("Failed to read the field!\n");
                    return;
                }

                // A very simple that computes the average value of the field
            /*var sum: Float = 0;
                for(var i = 0; i < ni * nj * nk; i++) {
                    // We don't bother with the organization of the elements within
                    // the array since we want to access all the elements, but
                    // if we wanted to access columns and rows, we have to consider
                    // that the data is organized in column-major instead of row-major
                    sum += field[i];
                }
                // Please note that this example does not take missing values into account
                print("\tAverage value = %f\n", sum / (ni * nj * nk) );*/

                // Load grid definition
                /*int gridId = c_ezqkdef(ni, nj, grtyp, ig1, ig2, ig3, ig4, unitNb);

                // Print the lat/lon coordinates of each gridpoint
                for (int32_t i = 0; i < ni; i++) {
                    for (int32_t j = 0; j < nj; j++) {
                        // Get the lat/lon coordinates of a given grid point
                        float iFloat = i;
                        float jFloat = j;

                        // This function expects 1 <= x <= NI and 1 <= y <= NJ
                        // c_gdllfxy can retreive arrays of coordinates; the last
                        // parameter is the number of points in the arrays.
                        c_gdllfxy(gridId, &lat, &lon, &iFloat, &jFloat, 1);

                        // Fix longitude to display it in a conventional way
                        if (lon > 180) lon -= 360;

                        printf("\t\t[%04d,%04d] = %03.5f, %03.5f\n", i, j, lat, lon);
                    }
                }

                // Free grid definition
                c_gdrls(gridId);*/

                key = c_fstinfx(key, unitNb, &ni, &nj, &nk, -1, "", -1, -1, -1, "", "");
            } while (key > 0);

            // Close the file
            c_fstfrm(unitNb);

            return;
    }
}
