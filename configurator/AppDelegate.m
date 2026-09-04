//Created by Salty on 8/25/26.

#import "AppDelegate.h"
#import "Controllers/PPMainViewController.h"

@interface AppDelegate ()
@property (strong) PPMainViewController *mainViewController;
@property (strong) IBOutlet NSWindow *window;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    self.mainViewController = [[PPMainViewController alloc] initWithNibName:@"PPMainViewView"
                                                                     bundle:nil];
    
    if (!self.mainViewController)
        return;
    
    [self.mainViewController.view setFrame:self.window.contentView.bounds];
    [self.window.contentView setSubviews:@[self.mainViewController.view]];
}


- (void)applicationWillTerminate:(NSNotification *)aNotification {

}


- (BOOL)applicationSupportsSecureRestorableState:(NSApplication *)app {
    return YES;
}

- (BOOL)applicationShouldHandleReopen:(NSApplication *)sender hasVisibleWindows:(BOOL)hasVisibleWindows{
    if (!hasVisibleWindows)
        [self.window makeKeyAndOrderFront:nil];
    
    return YES;
}

@end
